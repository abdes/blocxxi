//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

#include <Nova/Base/Uuid.h>

namespace {

constexpr auto kCanonicalStringSize = std::size_t { 36U };

struct ThreadState final {
  uint64_t last_timestamp_ms = 0U;
  uint16_t sequence_counter = 0U;
  std::mt19937_64 rng;
};

auto MakeThreadRng() -> std::mt19937_64
{
  auto rd = std::random_device {};
  auto seed_words = std::array<uint32_t, 8> {}; // NOLINT(*-magic-numbers)
  std::ranges::generate(seed_words, [&rd]() -> uint32_t { return rd(); });
  auto seed = std::seed_seq(seed_words.begin(), seed_words.end());
  return std::mt19937_64(seed);
}

auto CurrentUnixTimestampMs() -> uint64_t
{
  const auto now = std::chrono::system_clock::now();
  const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
    now.time_since_epoch());
  const auto count = duration.count();
  if (count < 0) {
    return 0U;
  }
  return static_cast<uint64_t>(count);
}
// NOLINTBEGIN(*-magic-numbers)
constexpr auto ParseHexNibble(const char c) noexcept -> uint8_t
{
  constexpr auto kInvalidNibble = uint8_t { 0xFFU };
  if (c >= '0' && c <= '9') {
    return static_cast<uint8_t>(c - '0');
  }
  if (c >= 'a' && c <= 'f') {
    return static_cast<uint8_t>(c - 'a' + 10);
  }
  if (c >= 'A' && c <= 'F') {
    return static_cast<uint8_t>(c - 'A' + 10);
  }
  return kInvalidNibble;
}
// NOLINTEND(*-magic-numbers)

constexpr auto HasCanonicalHyphens(const std::string_view str) noexcept -> bool
{
  constexpr auto kHyphenPositions
    = std::array<std::size_t, 4> { 8U, 13U, 18U, 23U };
  return std::ranges::all_of(kHyphenPositions,
    [str](const auto position) -> auto { return str[position] == '-'; });
}

constexpr auto HasOnlyLowercaseHexDigits(const std::string_view str) noexcept
  -> bool
{
  return std::ranges::all_of(str, [](const char c) noexcept -> bool {
    return c == '-' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
  });
}

} // namespace

namespace nova {

// NOLINTBEGIN(*-magic-numbers)
// UUIDv7 bit layout values are intentionally left as RFC literals for clarity.

/*!
 Generates an RFC 9562 version 7 UUID.

 The generated value encodes Unix epoch milliseconds in the high 48 bits,
 then fills `rand_a` and `rand_b` with a thread-local sequence + RNG stream.

 @return A canonical UUIDv7 value.

 @note On sequence rollover within the same millisecond, timestamp advances by
 one millisecond to preserve monotonic ordering.
 @warning This API intentionally generates only version 7 UUIDs.
 @see Uuid::FromString, Uuid::ToString
*/
auto Uuid::Generate() -> Uuid
{
  thread_local auto state = ThreadState {
    .last_timestamp_ms = 0U,
    .sequence_counter = 0U,
    .rng = MakeThreadRng(),
  };

  auto current_ms = CurrentUnixTimestampMs();
  current_ms = (std::max)(current_ms, state.last_timestamp_ms);

  if (current_ms == state.last_timestamp_ms) {
    if (state.sequence_counter == 0x0FFFU) {
      current_ms = state.last_timestamp_ms + 1U;
      state.sequence_counter = 0U;
    } else {
      ++state.sequence_counter;
    }
  } else {
    state.sequence_counter = 0U;
  }
  state.last_timestamp_ms = current_ms;

  const auto timestamp_ms = current_ms & 0x0000FFFFFFFFFFFFULL;
  const auto rand_b = state.rng();

  auto bytes = std::array<uint8_t, Uuid::kSize> {};
  bytes[0] = static_cast<uint8_t>((timestamp_ms >> 40U) & 0xFFU);
  bytes[1] = static_cast<uint8_t>((timestamp_ms >> 32U) & 0xFFU);
  bytes[2] = static_cast<uint8_t>((timestamp_ms >> 24U) & 0xFFU);
  bytes[3] = static_cast<uint8_t>((timestamp_ms >> 16U) & 0xFFU);
  bytes[4] = static_cast<uint8_t>((timestamp_ms >> 8U) & 0xFFU);
  bytes[5] = static_cast<uint8_t>(timestamp_ms & 0xFFU);

  bytes[6] = static_cast<uint8_t>((state.sequence_counter >> 8U) & 0x0FU);
  bytes[7] = static_cast<uint8_t>(state.sequence_counter & 0xFFU);

  bytes[8] = static_cast<uint8_t>((rand_b >> 56U) & 0xFFU);
  bytes[9] = static_cast<uint8_t>((rand_b >> 48U) & 0xFFU);
  bytes[10] = static_cast<uint8_t>((rand_b >> 40U) & 0xFFU);
  bytes[11] = static_cast<uint8_t>((rand_b >> 32U) & 0xFFU);
  bytes[12] = static_cast<uint8_t>((rand_b >> 24U) & 0xFFU);
  bytes[13] = static_cast<uint8_t>((rand_b >> 16U) & 0xFFU);
  bytes[14] = static_cast<uint8_t>((rand_b >> 8U) & 0xFFU);
  bytes[15] = static_cast<uint8_t>(rand_b & 0xFFU);

  bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0FU) | 0x70U);
  bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3FU) | 0x80U);

  return Uuid { bytes };
}

auto Uuid::FromBytes(const std::array<std::uint8_t, kSize>& bytes)
  -> Result<Uuid>
{
  const auto candidate = Uuid { bytes };
  if (!candidate.IsValidV7()) {
    return Result<Uuid>::Err(std::errc::invalid_argument);
  }

  return Result<Uuid>::Ok(candidate);
}

/*!
 Parses and validates a canonical UUIDv7 string.

 Expected format is `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`.
 Validation enforces:
 - Exact length and hyphen placement.
 - Hexadecimal character validity.
 - RFC 9562 version 7 nibble.
 - RFC 4122 variant bits.

 @param str UUID text to parse.
 @return `Result<Uuid>::Ok(...)` on success, otherwise
 `Result<Uuid>::Err(std::errc::invalid_argument)`.

 @note Parsing is strict and rejects non-canonical structural forms.
 @see Uuid::Generate, Uuid::ToString
*/
auto Uuid::FromString(const std::string_view str) -> Result<Uuid>
{
  constexpr auto kInvalidNibble = uint8_t { 0xFFU };

  if (str.size() != kCanonicalStringSize || !HasCanonicalHyphens(str)
    || !HasOnlyLowercaseHexDigits(str)) {
    return Result<Uuid>::Err(std::errc::invalid_argument);
  }

  auto bytes = std::array<uint8_t, Uuid::kSize> {};
  auto byte_index = std::size_t { 0U };

  for (auto i = std::size_t { 0U }; i < str.size();) {
    if (str[i] == '-') {
      ++i;
      continue;
    }
    if ((i + 1U) >= str.size() || str[i + 1U] == '-') {
      return Result<Uuid>::Err(std::errc::invalid_argument);
    }

    const auto high = ParseHexNibble(str[i]);
    const auto low = ParseHexNibble(str[i + 1U]);
    if (high == kInvalidNibble || low == kInvalidNibble) {
      return Result<Uuid>::Err(std::errc::invalid_argument);
    }

    if (byte_index >= bytes.size()) {
      return Result<Uuid>::Err(std::errc::invalid_argument);
    }

    bytes.at(byte_index) = static_cast<uint8_t>((high << 4U) | low);
    ++byte_index;
    i += 2U;
  }

  if (byte_index != bytes.size()) {
    return Result<Uuid>::Err(std::errc::invalid_argument);
  }

  return FromBytes(bytes);
}

/*!
 Converts this UUID value into canonical lower-case text.

 @return Canonical UUID string in `8-4-4-4-12` format.
 @see to_string(const Uuid&)
*/
auto Uuid::ToString() const -> std::string { return to_string(*this); }

/*!
 Converts a UUID value into canonical lower-case text.

 The emitted representation always uses lowercase hex and the `8-4-4-4-12`
 separator layout.

 @param uuid UUID value to format.
 @return Canonical UUID string representation.
 @see Uuid::ToString
*/
auto to_string(const Uuid& uuid) -> std::string
{
  auto text = std::string {};
  text.reserve(kCanonicalStringSize);

  const auto bytes = as_bytes(uuid);
  for (auto byte_index = std::size_t { 0U }; const auto byte : bytes) {
    if (byte_index == 4U || byte_index == 6U || byte_index == 8U
      || byte_index == 10U) {
      text.push_back('-');
    }

    fmt::format_to(
      std::back_inserter(text), "{:02x}", std::to_integer<uint8_t>(byte));
    ++byte_index;
  }

  return text;
}
// NOLINTEND(*-magic-numbers)

} // namespace nova
