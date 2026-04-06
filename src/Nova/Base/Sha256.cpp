//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Nova/Base/Sha256.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>

// SHA-NI intrinsics are intentional here for the Windows x86/x64 fast path.
// NOLINTBEGIN(*-magic-numbers,portability-simd-intrinsics,*-reinterpret-cast)
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#  include <immintrin.h>
#  include <intrin.h>
#  define NOVA_SHA_HAS_SHANI 1
#else
#  define NOVA_SHA_HAS_SHANI 0
#endif

namespace nova {

namespace {

  // SHA-256 round constants
  alignas(64) constexpr std::array<uint32_t, 64> kK = {
    0x428a2f98U,
    0x71374491U,
    0xb5c0fbcfU,
    0xe9b5dba5U,
    0x3956c25bU,
    0x59f111f1U,
    0x923f82a4U,
    0xab1c5ed5U,
    0xd807aa98U,
    0x12835b01U,
    0x243185beU,
    0x550c7dc3U,
    0x72be5d74U,
    0x80deb1feU,
    0x9bdc06a7U,
    0xc19bf174U,
    0xe49b69c1U,
    0xefbe4786U,
    0x0fc19dc6U,
    0x240ca1ccU,
    0x2de92c6fU,
    0x4a7484aaU,
    0x5cb0a9dcU,
    0x76f988daU,
    0x983e5152U,
    0xa831c66dU,
    0xb00327c8U,
    0xbf597fc7U,
    0xc6e00bf3U,
    0xd5a79147U,
    0x06ca6351U,
    0x14292967U,
    0x27b70a85U,
    0x2e1b2138U,
    0x4d2c6dfcU,
    0x53380d13U,
    0x650a7354U,
    0x766a0abbU,
    0x81c2c92eU,
    0x92722c85U,
    0xa2bfe8a1U,
    0xa81a664bU,
    0xc24b8b70U,
    0xc76c51a3U,
    0xd192e819U,
    0xd6990624U,
    0xf40e3585U,
    0x106aa070U,
    0x19a4c116U,
    0x1e376c08U,
    0x2748774cU,
    0x34b0bcb5U,
    0x391c0cb3U,
    0x4ed8aa4aU,
    0x5b9cca4fU,
    0x682e6ff3U,
    0x748f82eeU,
    0x78a5636fU,
    0x84c87814U,
    0x8cc70208U,
    0x90befffaU,
    0xa4506cebU,
    0xbef9a3f7U,
    0xc67178f2U,
  };

  // Initial hash values
  constexpr std::array<uint32_t, 8> kInitState = {
    0x6a09e667U,
    0xbb67ae85U,
    0x3c6ef372U,
    0xa54ff53aU,
    0x510e527fU,
    0x9b05688cU,
    0x1f83d9abU,
    0x5be0cd19U,
  };

  // Rotate right
  [[nodiscard]] constexpr auto RotR(uint32_t x, uint32_t n) noexcept -> uint32_t
  {
    return (x >> n) | (x << (32U - n));
  }

  // SHA-256 functions
  [[nodiscard]] constexpr auto Ch(uint32_t x, uint32_t y, uint32_t z) noexcept
    -> uint32_t
  {
    return (x & y) ^ ((~x) & z);
  }

  [[nodiscard]] constexpr auto Maj(uint32_t x, uint32_t y, uint32_t z) noexcept
    -> uint32_t
  {
    return (x & y) ^ (x & z) ^ (y & z);
  }

  [[nodiscard]] constexpr auto Sigma0(uint32_t x) noexcept -> uint32_t
  {
    return RotR(x, 2) ^ RotR(x, 13) ^ RotR(x, 22);
  }

  [[nodiscard]] constexpr auto Sigma1(uint32_t x) noexcept -> uint32_t
  {
    return RotR(x, 6) ^ RotR(x, 11) ^ RotR(x, 25);
  }

  [[nodiscard]] constexpr auto sigma0(uint32_t x) noexcept -> uint32_t
  {
    return RotR(x, 7) ^ RotR(x, 18) ^ (x >> 3);
  }

  [[nodiscard]] constexpr auto sigma1(uint32_t x) noexcept -> uint32_t
  {
    return RotR(x, 17) ^ RotR(x, 19) ^ (x >> 10);
  }

  // Read 32-bit big-endian value
  [[nodiscard]] inline auto LoadBE32(const std::byte* ptr) noexcept -> uint32_t
  {
    const auto* p = reinterpret_cast<const uint8_t*>(ptr);
    return (static_cast<uint32_t>(p[0]) << 24U)
      | (static_cast<uint32_t>(p[1]) << 16U)
      | (static_cast<uint32_t>(p[2]) << 8U) | static_cast<uint32_t>(p[3]);
  }

  // Write 32-bit big-endian value
  inline auto StoreBE32(uint8_t* ptr, uint32_t val) noexcept -> void
  {
    ptr[0] = static_cast<uint8_t>(val >> 24U);
    ptr[1] = static_cast<uint8_t>(val >> 16U);
    ptr[2] = static_cast<uint8_t>(val >> 8U);
    ptr[3] = static_cast<uint8_t>(val);
  }

  // Write 64-bit big-endian value
  inline auto StoreBE64(uint8_t* ptr, uint64_t val) noexcept -> void
  {
    ptr[0] = static_cast<uint8_t>(val >> 56U);
    ptr[1] = static_cast<uint8_t>(val >> 48U);
    ptr[2] = static_cast<uint8_t>(val >> 40U);
    ptr[3] = static_cast<uint8_t>(val >> 32U);
    ptr[4] = static_cast<uint8_t>(val >> 24U);
    ptr[5] = static_cast<uint8_t>(val >> 16U);
    ptr[6] = static_cast<uint8_t>(val >> 8U);
    ptr[7] = static_cast<uint8_t>(val);
  }

#if NOVA_SHA_HAS_SHANI
  // Cached CPU feature detection
  [[nodiscard]] auto DetectShaNi() noexcept -> bool
  {
    int cpu_info[4] = {};
    __cpuid(cpu_info, 0);
    const int max_leaf = cpu_info[0];
    if (max_leaf < 7) {
      return false;
    }
    __cpuidex(cpu_info, 7, 0);
    // SHA-NI is bit 29 of EBX
    return (cpu_info[1] & (1 << 29)) != 0;
  }

  [[nodiscard]] auto HasShaNi() noexcept -> bool
  {
    static const bool kHasShaNi = DetectShaNi();
    return kHasShaNi;
  }
#endif

  // One SHA-256 round (software)
  inline auto Round(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d,
    uint32_t& e, uint32_t& f, uint32_t& g, uint32_t& h, uint32_t k,
    uint32_t w) noexcept -> void
  {
    const uint32_t t1 = h + Sigma1(e) + Ch(e, f, g) + k + w;
    const uint32_t t2 = Sigma0(a) + Maj(a, b, c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

} // namespace

Sha256::Sha256() noexcept
  : state_(kInitState)
{
}

auto Sha256::HasHardwareSupport() noexcept -> bool
{
#if NOVA_SHA_HAS_SHANI
  return HasShaNi();
#else
  return false;
#endif
}

auto Sha256::Update(std::span<const std::byte> data) noexcept -> void
{
  if (data.empty()) {
    return;
  }

  const auto* ptr = data.data();
  size_t remaining = data.size();

  total_bytes_ += remaining;

  // If we have buffered data, try to complete a block
  if (buffer_size_ != 0) {
    const size_t need = kBlockSize - buffer_size_;
    const size_t take = std::min(need, remaining);
    std::memcpy(buffer_.data() + buffer_size_, ptr, take);
    buffer_size_ += take;
    ptr += take;
    remaining -= take;

    if (buffer_size_ == kBlockSize) {
      ProcessBlocks_(buffer_.data(), 1);
      buffer_size_ = 0;
    }
  }

  // Process complete blocks directly from input
  if (remaining >= kBlockSize) {
    const size_t blocks = remaining / kBlockSize;
    ProcessBlocks_(ptr, blocks);
    const size_t processed = blocks * kBlockSize;
    ptr += processed;
    remaining -= processed;
  }

  // Buffer remaining bytes
  if (remaining != 0) {
    std::memcpy(buffer_.data(), ptr, remaining);
    buffer_size_ = remaining;
  }
}

auto Sha256::Finalize() noexcept -> Sha256Digest
{
  // Pad message
  const uint64_t total_bits = total_bytes_ * 8;

  // Append 0x80 byte
  buffer_[buffer_size_++] = std::byte { 0x80 };

  // If not enough room for length, pad and process
  if (buffer_size_ > 56) {
    std::memset(buffer_.data() + buffer_size_, 0, kBlockSize - buffer_size_);
    ProcessBlocks_(buffer_.data(), 1);
    buffer_size_ = 0;
  }

  // Pad to 56 bytes and append length
  std::memset(buffer_.data() + buffer_size_, 0, 56 - buffer_size_);
  StoreBE64(reinterpret_cast<uint8_t*>(buffer_.data() + 56), total_bits);
  ProcessBlocks_(buffer_.data(), 1);

  // Output digest
  Sha256Digest digest = {};
  for (size_t i = 0; i < 8; ++i) {
    StoreBE32(digest.data() + (i * 4), state_[i]);
  }

  // Reset state for potential reuse
  state_ = kInitState;
  total_bytes_ = 0;
  buffer_size_ = 0;

  return digest;
}

auto Sha256::ProcessBlocks_(const std::byte* data, size_t block_count) noexcept
  -> void
{
#if NOVA_SHA_HAS_SHANI
  if (HasShaNi()) {
    ProcessBlocksShaNi_(data, block_count);
    return;
  }
#endif
  ProcessBlocksSoftware_(data, block_count);
}

// Optimized software implementation with partial loop unrolling
auto Sha256::ProcessBlocksSoftware_(
  const std::byte* data, size_t block_count) noexcept -> void
{
  alignas(64) std::array<uint32_t, 64> w {};

  for (size_t blk = 0; blk < block_count; ++blk) {
    const std::byte* block = data + (blk * kBlockSize);

    // Load and expand message schedule
    for (size_t i = 0; i < 16; ++i) {
      w[i] = LoadBE32(block + (i * 4));
    }
    for (size_t i = 16; i < 64; ++i) {
      w[i] = sigma1(w[i - 2]) + w[i - 7] + sigma0(w[i - 15]) + w[i - 16];
    }

    // Initialize working variables
    uint32_t a = state_[0];
    uint32_t b = state_[1];
    uint32_t c = state_[2];
    uint32_t d = state_[3];
    uint32_t e = state_[4];
    uint32_t f = state_[5];
    uint32_t g = state_[6];
    uint32_t h = state_[7];

    // 64 rounds, unrolled 8 at a time
    for (size_t i = 0; i < 64; i += 8) {
      Round(a, b, c, d, e, f, g, h, kK[i + 0], w[i + 0]);
      Round(a, b, c, d, e, f, g, h, kK[i + 1], w[i + 1]);
      Round(a, b, c, d, e, f, g, h, kK[i + 2], w[i + 2]);
      Round(a, b, c, d, e, f, g, h, kK[i + 3], w[i + 3]);
      Round(a, b, c, d, e, f, g, h, kK[i + 4], w[i + 4]);
      Round(a, b, c, d, e, f, g, h, kK[i + 5], w[i + 5]);
      Round(a, b, c, d, e, f, g, h, kK[i + 6], w[i + 6]);
      Round(a, b, c, d, e, f, g, h, kK[i + 7], w[i + 7]);
    }

    // Add compressed chunk to current hash value
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
  }
}

#if NOVA_SHA_HAS_SHANI
// Intel SHA-NI hardware-accelerated implementation
auto Sha256::ProcessBlocksShaNi_(
  const std::byte* data, size_t block_count) noexcept -> void
{
  // SHA-NI requires these constants for byte shuffling to big-endian
  const __m128i kShufMask
    = _mm_set_epi64x(0x0c0d0e0f08090a0bULL, 0x0405060700010203ULL);

  // Load initial state: state_[0..7] = A B C D E F G H
  // In memory as 128-bit loads:
  //   tmp0 = lanes[3][2][1][0] = D C B A
  //   tmp1 = lanes[3][2][1][0] = H G F E
  __m128i tmp0
    = _mm_loadu_si128(reinterpret_cast<const __m128i*>(state_.data()));
  __m128i tmp1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&state_[4]));

  // SHA-NI expects: STATE0 = [A][B][E][F], STATE1 = [C][D][G][H] in lanes
  // [3][2][1][0] Swap pairs: DCBA -> CDAB, HGFE -> GHEF
  tmp0 = _mm_shuffle_epi32(tmp0, 0xB1); // C D A B
  tmp1 = _mm_shuffle_epi32(tmp1, 0xB1); // G H E F

  // Interleave: STATE0 gets [A][B] from tmp0 lower and [E][F] from tmp1 lower
  //             STATE1 gets [C][D] from tmp0 upper and [G][H] from tmp1 upper
  __m128i state0 = _mm_unpacklo_epi64(tmp1, tmp0); // [A][B][E][F]
  __m128i state1 = _mm_unpackhi_epi64(tmp1, tmp0); // [C][D][G][H]
  __m128i tmp;

  for (size_t blk = 0; blk < block_count; ++blk) {
    const __m128i* msg_ptr
      = reinterpret_cast<const __m128i*>(data + (blk * kBlockSize));

    // Save current state
    const __m128i save_state0 = state0;
    const __m128i save_state1 = state1;

    // Load message and convert to big-endian
    __m128i msg0 = _mm_shuffle_epi8(_mm_loadu_si128(msg_ptr + 0), kShufMask);
    __m128i msg1 = _mm_shuffle_epi8(_mm_loadu_si128(msg_ptr + 1), kShufMask);
    __m128i msg2 = _mm_shuffle_epi8(_mm_loadu_si128(msg_ptr + 2), kShufMask);
    __m128i msg3 = _mm_shuffle_epi8(_mm_loadu_si128(msg_ptr + 3), kShufMask);

    // Rounds 0-3
    __m128i msg = _mm_add_epi32(
      msg0, _mm_loadu_si128(reinterpret_cast<const __m128i*>(kK.data())));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
    msg = _mm_shuffle_epi32(msg, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg);

    // Rounds 4-7
    msg = _mm_add_epi32(
      msg1, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&kK[4])));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
    msg = _mm_shuffle_epi32(msg, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg);
    msg0 = _mm_sha256msg1_epu32(msg0, msg1);

    // Rounds 8-11
    msg = _mm_add_epi32(
      msg2, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&kK[8])));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
    msg = _mm_shuffle_epi32(msg, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg);
    msg1 = _mm_sha256msg1_epu32(msg1, msg2);

    // Rounds 12-15
    msg = _mm_add_epi32(
      msg3, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&kK[12])));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
    tmp = _mm_alignr_epi8(msg3, msg2, 4);
    msg0 = _mm_add_epi32(msg0, tmp);
    msg0 = _mm_sha256msg2_epu32(msg0, msg3);
    msg = _mm_shuffle_epi32(msg, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg);
    msg2 = _mm_sha256msg1_epu32(msg2, msg3);

    // Rounds 16-19
    msg = _mm_add_epi32(
      msg0, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&kK[16])));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
    tmp = _mm_alignr_epi8(msg0, msg3, 4);
    msg1 = _mm_add_epi32(msg1, tmp);
    msg1 = _mm_sha256msg2_epu32(msg1, msg0);
    msg = _mm_shuffle_epi32(msg, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg);
    msg3 = _mm_sha256msg1_epu32(msg3, msg0);

    // Rounds 20-23
    msg = _mm_add_epi32(
      msg1, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&kK[20])));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
    tmp = _mm_alignr_epi8(msg1, msg0, 4);
    msg2 = _mm_add_epi32(msg2, tmp);
    msg2 = _mm_sha256msg2_epu32(msg2, msg1);
    msg = _mm_shuffle_epi32(msg, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg);
    msg0 = _mm_sha256msg1_epu32(msg0, msg1);

    // Rounds 24-27
    msg = _mm_add_epi32(
      msg2, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&kK[24])));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
    tmp = _mm_alignr_epi8(msg2, msg1, 4);
    msg3 = _mm_add_epi32(msg3, tmp);
    msg3 = _mm_sha256msg2_epu32(msg3, msg2);
    msg = _mm_shuffle_epi32(msg, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg);
    msg1 = _mm_sha256msg1_epu32(msg1, msg2);

    // Rounds 28-31
    msg = _mm_add_epi32(
      msg3, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&kK[28])));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
    tmp = _mm_alignr_epi8(msg3, msg2, 4);
    msg0 = _mm_add_epi32(msg0, tmp);
    msg0 = _mm_sha256msg2_epu32(msg0, msg3);
    msg = _mm_shuffle_epi32(msg, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg);
    msg2 = _mm_sha256msg1_epu32(msg2, msg3);

    // Rounds 32-35
    msg = _mm_add_epi32(
      msg0, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&kK[32])));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
    tmp = _mm_alignr_epi8(msg0, msg3, 4);
    msg1 = _mm_add_epi32(msg1, tmp);
    msg1 = _mm_sha256msg2_epu32(msg1, msg0);
    msg = _mm_shuffle_epi32(msg, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg);
    msg3 = _mm_sha256msg1_epu32(msg3, msg0);

    // Rounds 36-39
    msg = _mm_add_epi32(
      msg1, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&kK[36])));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
    tmp = _mm_alignr_epi8(msg1, msg0, 4);
    msg2 = _mm_add_epi32(msg2, tmp);
    msg2 = _mm_sha256msg2_epu32(msg2, msg1);
    msg = _mm_shuffle_epi32(msg, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg);
    msg0 = _mm_sha256msg1_epu32(msg0, msg1);

    // Rounds 40-43
    msg = _mm_add_epi32(
      msg2, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&kK[40])));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
    tmp = _mm_alignr_epi8(msg2, msg1, 4);
    msg3 = _mm_add_epi32(msg3, tmp);
    msg3 = _mm_sha256msg2_epu32(msg3, msg2);
    msg = _mm_shuffle_epi32(msg, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg);
    msg1 = _mm_sha256msg1_epu32(msg1, msg2);

    // Rounds 44-47
    msg = _mm_add_epi32(
      msg3, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&kK[44])));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
    tmp = _mm_alignr_epi8(msg3, msg2, 4);
    msg0 = _mm_add_epi32(msg0, tmp);
    msg0 = _mm_sha256msg2_epu32(msg0, msg3);
    msg = _mm_shuffle_epi32(msg, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg);
    msg2 = _mm_sha256msg1_epu32(msg2, msg3);

    // Rounds 48-51
    msg = _mm_add_epi32(
      msg0, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&kK[48])));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
    tmp = _mm_alignr_epi8(msg0, msg3, 4);
    msg1 = _mm_add_epi32(msg1, tmp);
    msg1 = _mm_sha256msg2_epu32(msg1, msg0);
    msg = _mm_shuffle_epi32(msg, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg);
    msg3 = _mm_sha256msg1_epu32(msg3, msg0);

    // Rounds 52-55
    msg = _mm_add_epi32(
      msg1, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&kK[52])));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
    tmp = _mm_alignr_epi8(msg1, msg0, 4);
    msg2 = _mm_add_epi32(msg2, tmp);
    msg2 = _mm_sha256msg2_epu32(msg2, msg1);
    msg = _mm_shuffle_epi32(msg, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg);

    // Rounds 56-59
    msg = _mm_add_epi32(
      msg2, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&kK[56])));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
    tmp = _mm_alignr_epi8(msg2, msg1, 4);
    msg3 = _mm_add_epi32(msg3, tmp);
    msg3 = _mm_sha256msg2_epu32(msg3, msg2);
    msg = _mm_shuffle_epi32(msg, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg);

    // Rounds 60-63
    msg = _mm_add_epi32(
      msg3, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&kK[60])));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
    msg = _mm_shuffle_epi32(msg, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg);

    // Add saved state
    state0 = _mm_add_epi32(state0, save_state0);
    state1 = _mm_add_epi32(state1, save_state1);
  }

  // Convert back: STATE0 = [A][B][E][F], STATE1 = [C][D][G][H]
  // Need: tmp0 = [D][C][B][A], tmp1 = [H][G][F][E] for storage
  // First get [C][D][A][B] and [G][H][E][F]
  tmp0 = _mm_unpackhi_epi64(state0, state1); // [C][D][A][B]
  tmp1 = _mm_unpacklo_epi64(state0, state1); // [G][H][E][F]

  // Swap pairs back: [C][D][A][B] -> [D][C][B][A], [G][H][E][F] -> [H][G][F][E]
  tmp0 = _mm_shuffle_epi32(tmp0, 0xB1); // D C B A
  tmp1 = _mm_shuffle_epi32(tmp1, 0xB1); // H G F E

  _mm_storeu_si128(reinterpret_cast<__m128i*>(state_.data()), tmp0);
  _mm_storeu_si128(reinterpret_cast<__m128i*>(&state_[4]), tmp1);
}
#endif

auto ComputeSha256(std::span<const std::byte> data) noexcept -> Sha256Digest
{
  Sha256 hasher;
  hasher.Update(data);
  return hasher.Finalize();
}

auto IsAllZero(const Sha256Digest& digest) noexcept -> bool
{
  // Use a constant-time comparison to avoid timing attacks
  uint8_t acc = 0;
  for (const auto b : digest) {
    acc |= b;
  }
  return acc == 0;
}

auto ComputeFileSha256(const std::filesystem::path& path) -> Sha256Digest
{
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error(
      "Failed to open file for SHA-256: " + path.string());
  }

  Sha256 hasher;

  // Use larger buffer for better I/O performance (256 KB)
  constexpr size_t kBufferSize = static_cast<size_t>(256) * 1024;
  std::array<std::byte, kBufferSize> buffer = {};

  while (true) {
    in.read(reinterpret_cast<char*>(buffer.data()),
      static_cast<std::streamsize>(buffer.size()));
    const auto got = in.gcount();
    if (got > 0) {
      hasher.Update(
        std::span<const std::byte>(buffer.data(), static_cast<size_t>(got)));
    }

    if (!in) {
      if (in.eof()) {
        break;
      }
      throw std::runtime_error(
        "Failed while reading file for SHA-256: " + path.string());
    }
  }

  return hasher.Finalize();
}

} // namespace nova

// NOLINTEND(*-magic-numbers, portability-simd-intrinsics,*-reinterpret-cast)
