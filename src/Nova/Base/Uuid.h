//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//
#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

#include <fmt/format.h>

#include <Nova/Base/Hash.h>
#include <Nova/Base/Macros.h>
#include <Nova/Base/Result.h>
#include <Nova/Base/api_export.h>

namespace nova {

//! Represents a 128-bit UUID value constrained to RFC 9562 version 7.
/*!
 The type stores UUID bytes in canonical RFC byte order and is
intentionally
 lightweight for hashing, ordering, and binary serialization.

### Key Features

- **UUIDv7-only ingress**: Public generation and validated parsing paths reject

non-v7 values.
- **Value semantics**: Trivially copyable, fixed-size storage.
-
**Container-friendly**: Supports ordering and hash specializations.

 @see to_string(const Uuid&), as_bytes(const Uuid&)
*/
class Uuid {
public:
  // Standard Container Type Aliases
  using value_type = std::uint8_t;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;

  static constexpr size_type kSize = 16U;

  using iterator = std::array<value_type, kSize>::iterator;
  using const_iterator = std::array<value_type, kSize>::const_iterator;
  using SpanType = std::span<const value_type, kSize>;

  //! Constructs a nil UUID (all bytes set to zero).
  constexpr Uuid() noexcept = default;

  ~Uuid() noexcept = default;

  NOVA_DEFAULT_COPYABLE(Uuid)
  NOVA_DEFAULT_MOVABLE(Uuid)

  //! Generates a new RFC 9562 UUIDv7 value.
  NOVA_BASE_NDAPI static auto Generate() -> Uuid;

  //! Parses and validates canonical UUIDv7 bytes.
  NOVA_BASE_NDAPI static auto FromBytes(
    const std::array<std::uint8_t, kSize>& bytes) -> Result<Uuid>;

  //! Parses a canonical v7 UUID text representation.
  NOVA_BASE_NDAPI static auto FromString(std::string_view str) -> Result<Uuid>;

  //! Returns the canonical lower-case UUID string (`8-4-4-4-12`).
  NOVA_BASE_NDAPI auto ToString() const -> std::string;

  //! Returns read-only access to the underlying 16-byte UUID storage.
  [[nodiscard]] constexpr auto data() const noexcept -> const std::uint8_t*
  {
    return bytes_.data();
  }

  //! Returns the UUID byte count (always `16`).
  [[nodiscard]] constexpr auto size() const noexcept -> std::size_t
  {
    return kSize;
  }

  //! Returns a const iterator to the first byte.
  [[nodiscard]] constexpr auto begin() const noexcept { return bytes_.begin(); }

  //! Returns a const iterator one past the last byte.
  [[nodiscard]] constexpr auto end() const noexcept { return bytes_.end(); }

  //! Returns the encoded UUID version nibble.
  [[nodiscard]] constexpr auto Version() const noexcept -> std::uint8_t
  {
    constexpr auto kVersionByteIndex = std::size_t { 6U };
    constexpr auto kVersionShift = uint8_t { 4U };
    return static_cast<std::uint8_t>(
      bytes_[kVersionByteIndex] >> kVersionShift);
  }

  //! Returns true when the UUID encodes RFC 9562 version 7.
  [[nodiscard]] constexpr auto IsVersion7() const noexcept -> bool
  {
    return Version() == 7U; // NOLINT
  }

  //! Returns true when the UUID encodes the RFC 4122 variant bits.
  [[nodiscard]] constexpr auto HasRfc4122Variant() const noexcept -> bool
  {
    constexpr auto kVariantByteIndex = std::size_t { 8U };
    return (bytes_[kVariantByteIndex] & 0xC0U) == 0x80U; // NOLINT
  }

  //! Returns true when the UUID is a non-nil RFC 9562 UUIDv7 value.
  [[nodiscard]] constexpr auto IsValidV7() const noexcept -> bool
  {
    return !IsNil() && IsVersion7() && HasRfc4122Variant();
  }

  //! Returns true when all UUID bytes are zero.
  [[nodiscard]] constexpr auto IsNil() const noexcept -> bool
  {
    return *this == Uuid {};
  }

  // Modern C++ standard spaceship comparison.
  // Facilitates correct time-ordered lexical sorting (RFC Sec 6.11).
  [[nodiscard]] friend constexpr auto operator<=>(
    const Uuid&, const Uuid&) noexcept -> std::strong_ordering = default;
  [[nodiscard]] friend constexpr auto operator==(
    const Uuid&, const Uuid&) noexcept -> bool = default;

private:
  //! Constructs a UUID from already-validated canonical bytes.
  constexpr explicit Uuid(const std::array<std::uint8_t, kSize>& bytes) noexcept
    : bytes_(bytes)
  {
  }

  //! Constructs a UUID from already-validated one-byte elements.
  template <typename T>
    requires(sizeof(T) == 1)
  constexpr explicit Uuid(std::span<const T, kSize> bytes) noexcept
  {
    auto out = bytes_.begin();
    for (const auto& b : bytes) {
      *out++ = static_cast<std::uint8_t>(b);
    }
  }

  std::array<std::uint8_t, kSize> bytes_ {};
};

// Compile-time ABI and Type Trait Assertions
static_assert(sizeof(Uuid) == Uuid::kSize, "Uuid must be exactly 16 bytes");
static_assert(std::is_trivially_copyable_v<Uuid>,
  "Uuid must be trivially copyable for raw serialization");
static_assert(std::is_standard_layout_v<Uuid>,
  "Uuid must have standard layout for Vulkan/D3D12 ABI compatibility");
static_assert(
  std::is_nothrow_move_constructible_v<Uuid>, "Uuid must not throw on move");
static_assert(
  std::is_nothrow_copy_constructible_v<Uuid>, "Uuid must not throw on copy");
static_assert(std::is_nothrow_move_assignable_v<Uuid>,
  "Uuid must not throw on move assign");
static_assert(std::is_nothrow_copy_assignable_v<Uuid>,
  "Uuid must not throw on copy assign");
static_assert(
  std::is_nothrow_destructible_v<Uuid>, "Uuid must not throw on destruction");

//! Converts a UUID to canonical lower-case text (`8-4-4-4-12`).
NOVA_BASE_NDAPI auto to_string(const Uuid& uuid) -> std::string;

//! Returns a read-only byte span view of UUID storage.
[[nodiscard]] inline auto as_bytes(const Uuid& uuid) noexcept
  -> std::span<const std::byte, Uuid::kSize>
{
  return std::as_bytes(
    std::span<const Uuid::value_type, Uuid::kSize>(uuid.data(), uuid.size()));
}

} // namespace nova

//------------------------------------------------------------------------------
// Standard Library & FMT Integrations
//------------------------------------------------------------------------------

template <> struct std::hash<nova::Uuid> {
  //! Hashes UUID bytes using FNV-1a helper.
  [[nodiscard]] auto operator()(const nova::Uuid& uuid) const noexcept
    -> std::size_t
  {
    return static_cast<std::size_t>(
      nova::ComputeFNV1a64(uuid.data(), uuid.size()));
  }
};

template <> struct fmt::formatter<nova::Uuid> : fmt::formatter<std::string> {
  //! Parses UUID format options (delegates to default string formatter).
  constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

  //! Formats UUID using canonical lower-case text representation.
  auto format(const nova::Uuid& uuid, fmt::format_context& ctx) const
  {
    return fmt::format_to(ctx.out(), "{}", nova::to_string(uuid));
  }
};
