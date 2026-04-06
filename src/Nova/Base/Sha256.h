//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

#include <Nova/Base/api_export.h>

namespace nova {

using Sha256Digest = std::array<uint8_t, 32>; // NOLINT(*-magic-numbers)

//! High-performance incremental SHA-256 implementation.
/*!
 Provides an optimized SHA-256 implementation with:
 - Hardware acceleration via Intel SHA-NI when available (5-10x faster)
 - Optimized software fallback with loop unrolling
 - Runtime CPU feature detection

 @note This API is used by higher-level modules (e.g. Content validation).
*/
class Sha256 final {
public:
  static constexpr size_t kDigestSize = 32;
  static constexpr size_t kBlockSize = 64;

  NOVA_BASE_API Sha256() noexcept;

  //! Update the hash with additional data (streaming interface).
  NOVA_BASE_API auto Update(std::span<const std::byte> data) noexcept -> void;

  //! Finalize and return the digest. Hasher state is reset after this call.
  NOVA_BASE_NDAPI auto Finalize() noexcept -> Sha256Digest;

  //! Check if hardware SHA-NI acceleration is available on this CPU.
  NOVA_BASE_NDAPI static auto HasHardwareSupport() noexcept -> bool;

private:
  auto ProcessBlocks_(const std::byte* data, size_t block_count) noexcept
    -> void;
  auto ProcessBlocksSoftware_(
    const std::byte* data, size_t block_count) noexcept -> void;
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
  auto ProcessBlocksShaNi_(const std::byte* data, size_t block_count) noexcept
    -> void;
#endif

  uint64_t total_bytes_ = 0;
  std::array<std::byte, kBlockSize> buffer_ = {};
  size_t buffer_size_ = 0;
  // NOLINTNEXTLINE(*-magic-numbers)
  alignas(16) std::array<uint32_t, 8> state_ = {};
};

//! Compute SHA-256 of a memory buffer in one call.
NOVA_BASE_NDAPI auto ComputeSha256(std::span<const std::byte> data) noexcept
  -> Sha256Digest;

//! Compute SHA-256 of a file.
NOVA_BASE_NDAPI auto ComputeFileSha256(const std::filesystem::path& path)
  -> Sha256Digest;

//! Check if a digest is all zeros.
NOVA_BASE_NDAPI auto IsAllZero(const Sha256Digest& digest) noexcept -> bool;

} // namespace nova
