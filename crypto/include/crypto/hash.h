//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <crypto/blocxxi_crypto_api.h>

#include <array>  // for std::array
#include <bitset> // for std::bitset
#include <cstddef>
#include <cstdint>  // for standard int types
#include <cstring>  // for std::memcpy
#include <iosfwd>   // for implementation of operator<<
#include <iterator> // for std::reverse_iterator
#include <string>   // for std::string

#include <gsl/gsl> // for gsl::span

#include <codec/base16.h>  // for hex enc/decoding
#include <crypto/random.h> // for random block generation

namespace blocxxi::crypto {

namespace detail {
/// Forward declarations for utility conversion function between host and
/// network (big endian) byte orders.
/// Avoid including header files of boost and pulling all its bloat here.
BLOCXXI_CRYPTO_API auto HostToNetwork(std::uint32_t) -> std::uint32_t;
BLOCXXI_CRYPTO_API auto NetworkToHost(std::uint32_t) -> std::uint32_t;

/// Count the number of leading zero bits in a buffer of contiguous 32-bit
/// integers.
BLOCXXI_CRYPTO_API auto CountLeadingZeroBits(gsl::span<std::uint32_t const> buf)
    -> size_t;

} // namespace detail

/*!
   Represents a N bits hash digest or any other kind of N bits sequence.
   The structure is 32-bit aligned and must be at least 32-bits wide. It
   has almost the same semantics of std::array<> except that it will never be
   zero-length (size() is always > 0). It offers a number of additional
   convenience methods, such as bitwise arithemtics, comparisons etc.
 */
template <unsigned int BITS> class Hash {
  static_assert(BITS % 32 == 0, "Hash size in bits must be a multiple of 32");
  static_assert(BITS > 0, "Hash size in bits must be greater than 0");

public:
  using value_type = std::uint8_t;
  using pointer = uint8_t *;
  using const_pointer = std::uint8_t const *;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = std::uint8_t &;
  using const_reference = std::uint8_t const &;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  ///@name Constructors and Factory methods
  //@{
  /// Initializes the hash with all '0' bits
  Hash() : storage_({}) {
  }

  Hash(const Hash &other) = default;

  auto operator=(const Hash &rhs) -> Hash & = default;

  Hash(Hash &&) noexcept = default;

  auto operator=(Hash &&rhs) noexcept -> Hash & = default;

  virtual ~Hash() = default;

  /*!
  \brief Construct a Hash by assigning content from the provided sequence of
  bytes.

  > The source sequence can be smaller than the hash size but it __cannot be
  > bigger__.

  In the case it is smaller, the hash will be padded (leading bytes)
  with 0. The last byte in the source sequence will be the LSB of the hash
  (last byte in the hash).

  In the case it is bigger, result is undefined and, if assertions are enabled,
  this would be considered as a fatal error.

  \param buf source sequence of bytes.
  */
  explicit Hash(gsl::span<std::uint8_t const> buf) noexcept : storage_({}) {
    auto src_size = buf.size();
    // TODO(Abdessattar): replace with contract
    // ASAP_ASSERT_PRECOND(src_size <= Size());
    auto start = begin();
    if (Size() > src_size) {
      // Pad with zeros and adjust the start
      std::memset(Data(), 0, Size() - src_size);
      start += Size() - src_size;
    }
    Assign(buf, start);
  }

  /// \brief Returns an all-1 hash, i.e. the biggest value representable by an N
  /// bit number (N/8 bytes).
  static auto Max() noexcept -> Hash {
    Hash hash;
    hash.storage_.fill(0xffffffffU);
    return hash;
  }

  /// \brief Returns an all-0 hash, i.e. the smallest value representable by an
  /// N bit number (N/8 bytes).
  static auto Min() noexcept -> Hash {
    return Hash();
    // all bits are already 0
  }

  static auto FromHex(const std::string &src, bool reverse = false)
      -> Hash<BITS> {
    Hash<BITS> hash;
    codec::hex::Decode(gsl::make_span<>(src),
        gsl::make_span<>(hash.Data(), hash.Size()), reverse);
    return hash;
  }

  static auto RandomHash() -> Hash<BITS> {
    Hash<BITS> hash;
    hash.Randomize();
    return hash;
  }
  //@}

  /// @name Checked element access
  /// access specified element with bounds checking.
  //@{
  /*!
    Returns a reference to the element at specified location `pos`, with
    bounds checking.

    \param pos position of the element to return
    \exception std::out_of_range if !(pos < size()).
   */
  auto At(size_type pos) -> reference {
    if (Size() <= pos) {
      throw std::out_of_range("Hash<BITS> index out of range");
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<pointer>(storage_.data())[pos];
  }
  [[nodiscard]] auto At(size_type pos) const -> const_reference {
    if (Size() <= pos) {
      throw std::out_of_range("Hash<BITS> index out of range");
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<const_pointer>(storage_.data())[pos];
  }
  //@}

  /// @name Unchecked element access
  /// access specified element __without__ bounds checking.
  //@{
  /// Returns a reference to the element at specified location pos. No bounds
  /// checking is performed.
  auto operator[](size_type pos) noexcept -> reference {
    // TODO(Abdessattar): replace with contract
    // ASAP_ASSERT_PRECOND(pos < Size());
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<pointer>(storage_.data())[pos];
  }
  auto operator[](size_type pos) const noexcept -> const_reference {
    // TODO(Abdessattar): replace with contract
    // ASAP_ASSERT_PRECOND(pos < Size());
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<pointer>(storage_.data())[pos];
  }

  /// Returns a reference to the first element in the container.
  auto Front() -> reference {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<pointer>(storage_.data())[0];
  }
  [[nodiscard]] auto Front() const -> const_reference {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<const_pointer>(storage_.data())[0];
  }

  /// Returns reference to the last element in the container.
  auto Back() -> reference {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<pointer>(storage_.data())[Size() - 1];
  }
  [[nodiscard]] auto Back() const -> const_reference {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<const_pointer>(storage_.data())[Size() - 1];
  }

  /// Returns pointer to the underlying array serving as element storage. The
  /// pointer is such that range [Data(); Data() + Size()) is always a valid
  /// range.
  auto Data() noexcept -> pointer {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<pointer>(storage_.data());
  }
  [[nodiscard]] auto Data() const noexcept -> const_pointer {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<const_pointer>(storage_.data());
  }
  //@}

  /// @name Iterators
  //@{
  /// Returns an iterator to the first element of the container.
  auto begin() noexcept -> iterator {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<pointer>(storage_.data());
  }
  [[nodiscard]] auto begin() const noexcept -> const_iterator {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<const_pointer>(storage_.data());
  }
  [[nodiscard]] auto cbegin() const noexcept -> const_iterator {
    return begin();
  }

  /// Returns an iterator to the element following the last element of the
  /// container. This element acts as a placeholder; attempting to access it
  /// results in undefined behavior.
  auto end() noexcept -> iterator {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<pointer>(storage_.data()) + Size();
  }
  [[nodiscard]] auto end() const noexcept -> const_iterator {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<const_pointer>(storage_.data()) + Size();
  }
  [[nodiscard]] auto cend() const noexcept -> const_iterator {
    return end();
  }

  /// Returns a reverse iterator to the first element of the reversed container.
  /// It corresponds to the last element of the non-reversed container.
  auto rbegin() noexcept -> reverse_iterator {
    return reverse_iterator(end());
  }
  [[nodiscard]] auto rbegin() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator(end());
  }
  [[nodiscard]] auto crbegin() const noexcept -> const_reverse_iterator {
    return rbegin();
  }

  /// Returns a reverse iterator to the element following the last element of
  /// the reversed container. It corresponds to the element preceding the first
  /// element of the non-reversed container. This element acts as a placeholder,
  /// attempting to access it results in undefined behavior.
  auto rend() noexcept -> reverse_iterator {
    return reverse_iterator(begin());
  }
  [[nodiscard]] auto rend() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator(begin());
  }
  [[nodiscard]] auto crend() const noexcept -> const_reverse_iterator {
    return rend();
  }
  //@}

  // Capacity

  /// The size of the hash in bytes.
  constexpr static auto Size() -> std::size_t {
    return BITS / 8;
  }

  /// The size of the hash in bytes.
  constexpr static auto BitSize() -> std::size_t {
    return BITS;
  }

  // Operations

  /// Set all bits to 0.
  void Clear() {
    storage_.fill(0);
  }

  /// Return true if all bits are set to 0.
  [[nodiscard]] auto IsAllZero() const -> bool {
    for (auto chunk : storage_) {
      if (chunk != 0) {
        return false;
      }
    }
    return true;
  }

  /// Exchanges the contents of the container with those of other. Does not
  /// cause iterators and references to associate with the other container.
  void swap(Hash &other) noexcept {
    storage_.swap(other.storage_);
  }

  /// Count leading zero bits.
  [[nodiscard]] auto LeadingZeroBits() const -> size_t {
    return detail::CountLeadingZeroBits(gsl::make_span(storage_));
  }

  /// @name Comparison operators
  /// The important thing to note here is that only two of these operators
  /// actually do anything, the others are just forwarding their arguments to
  /// either of these two to do the actual work (== and <). Therefore only these
  /// two are implemented as friends.
  //@{
  /// \brief Checks if the contents of `lhs` and `rhs` are equal, that is,
  /// whether each element in `lhs` compares equal with the element in `rhs` at
  /// the same position.
  friend auto operator==(const Hash<BITS> &lhs, const Hash<BITS> &rhs) -> bool {
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
  }
  /// \brief Compares the contents of `lhs` and `rhs` lexicographically. The
  /// comparison is performed by a function equivalent to
  /// `std::lexicographical_compare`.
  friend auto operator<(const Hash<BITS> &lhs, const Hash<BITS> &rhs) -> bool {
    for (std::size_t i = 0; i < SIZE_DWORD; ++i) {
      std::uint32_t const native_lhs = detail::NetworkToHost(lhs.storage_[i]);
      std::uint32_t const native_rhs = detail::NetworkToHost(rhs.storage_[i]);
      if (native_lhs < native_rhs) {
        return true;
      }
      if (native_lhs > native_rhs) {
        return false;
      }
    }
    return false;
  }
  //@}

  /// In-place bitwise XOR with the given other hash.
  auto operator^=(Hash const &other) noexcept -> Hash & {
    for (std::size_t i = 0; i < SIZE_DWORD; ++i) {
      storage_[i] ^= other.storage_[i];
    }
    return *this;
  }

  /*!
  Starting at the given position in the hash, assign contents from the given
  sequence of bytes.

  > Data represented by the span is expected to be organized in network byte
  > order (i.e. the byte returned by span.first() is the MSB (Most Significant
  > Byte) and therefore will go into the MSB of the Hash).

  The _span_ size can be smaller than the remaining space after _start_ in
  the hash but it __cannot be bigger__. If this is not true, result is undefined
  and, if assertions are enabled, this would be considered as a fatal error.

  \param buf source sequence of bytes.
  \param start position at which to assign the sequence in the hash.
  */
  void Assign(gsl::span<std::uint8_t const> buf, iterator start) noexcept {
    size_type src_size = buf.size();
    auto dst_size = static_cast<size_type>(end() - start);
    // TODO(Abdessattar): replace with contract
    // ASAP_ASSERT_PRECOND(src_size <= dst_size);
    std::memcpy(start, buf.data(), std::min(src_size, dst_size));
  }

  // TODO(Abdessattar): Replace the parameters with a span
  void Randomize() {
    random::GenerateBlock(Data(), Size());
  }

  [[nodiscard]] auto ToHex() const -> std::string {
    return codec::hex::Encode(gsl::make_span<>(Data(), Size()), false, true);
  }

  auto ToBitSet() const -> std::bitset<BITS> {
    std::bitset<BITS> bits; // [0,0,...,0]
    std::size_t shift_left = BITS;
    for (uint32_t num_part : storage_) {
      shift_left -= 32;
      num_part = detail::HostToNetwork(num_part);
      std::bitset<BITS> bs_part(num_part);
      bits |= bs_part << shift_left;
    }
    return bits;
  }

  [[nodiscard]] auto ToBitStringShort(std::size_t length = 32U) const
      -> std::string {
    auto truncate = false;
    if (length < BITS) {
      truncate = true;
      length -= 3;
    }
    auto str = ToBitSet().to_string().substr(0, length);
    if (truncate) {
      str.append("...");
    }
    return str;
  }

private:
  /// The size of the underlying data buffer in terms of 32-bit DWORD
  static constexpr std::size_t SIZE_DWORD = BITS / 32;

  /// The underlying data buffer is built as an array of unsigned 32-bit
  /// integers to facilitate the math operations. Note however that iteration
  /// over the Hash and random access to the data should always be presented as
  /// if it were a byte array layed out in network order (big endian).
  ///
  /// > First DWORD is the most significant 4 bytes.
  std::array<std::uint32_t, SIZE_DWORD> storage_;
};

/// @name Comparison operators
/// Additional Comparison operators (not needing to be friend)
//@{

template <unsigned int BITS>
inline auto operator!=(const Hash<BITS> &lhs, const Hash<BITS> &rhs) -> bool {
  return !(lhs == rhs);
}
template <unsigned int BITS>
inline auto operator>(const Hash<BITS> &lhs, const Hash<BITS> &rhs) -> bool {
  return rhs < lhs;
}
template <unsigned int BITS>
inline auto operator<=(const Hash<BITS> &lhs, const Hash<BITS> &rhs) -> bool {
  return !(lhs > rhs);
}
template <unsigned int BITS>
inline auto operator>=(const Hash<BITS> &lhs, const Hash<BITS> &rhs) -> bool {
  return !(lhs < rhs);
}
//@}

/// Bitwise XOR
template <unsigned int BITS>
inline auto operator^(Hash<BITS> lhs, Hash<BITS> const &rhs) noexcept
    -> Hash<BITS> {
  return lhs.operator^=(rhs);
}

// Specialized algorithms

/// Exchanges the contents of one with other. Does not cause
/// iterators and references to associate with the other container.
template <unsigned int BITS>
inline void swap(Hash<BITS> &lhs, Hash<BITS> &rhs) {
  lhs.swap(rhs);
}

/// Dump the hex-encoded contents of the hash to the stream.
template <unsigned int BITS>
inline auto operator<<(std::ostream &out, Hash<BITS> const &hash)
    -> std::ostream & {
  out << hash.ToHex();
  return out;
}

using Hash512 = Hash<512>; // 64 bytes
using Hash256 = Hash<256>; // 32 bytes
using Hash160 = Hash<160>; // 20 bytes

} // namespace blocxxi::crypto
