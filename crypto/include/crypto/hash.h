//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#ifndef BLOCXXI_CRYPTO_HASH_H_
#define BLOCXXI_CRYPTO_HASH_H_

#include <array>     // for std::array
#include <bitset>    // for std::bitset
#include <cstdint>   // for standard int types
#include <cstring>   // for std::memcpy
#include <iosfwd>    // for implementation of operator<<
#include <iterator>  // for std::reverse_iterator
#include <string>    // for std::string

#include <gsl/gsl>  // for gsl::span

#include <codec/base16.h>   // for hex enc/decoding
#include <common/assert.h>  // for assertions
#include <crypto/random.h>  // for random block generation

namespace blocxxi {
namespace crypto {

namespace detail {
/// Forward declarations for utility conversion function between host and
/// network (big endian) byte orders.
/// Avoid including header files of boost and pulling all its bloat here.
std::uint32_t HostToNetwork(std::uint32_t);
std::uint32_t NetworkToHost(std::uint32_t);

/// Count the number of leading zero bits in a buffer of contiguous 32-bit
/// integers.
int CountLeadingZeroBits(gsl::span<std::uint32_t const> buf);

}  // namespace detail

/*!
   Represents a N bits hash digest or any other kind of N bits sequence.
   The structure is 32-bit aligned and must be at least 32-bits wide. It
   has almost the same semantics of std::array<> except that it will never be
   zero-length (size() is always > 0). It offers a number of additional
   convenience methods, such as bitwise arithemtics, comparisons etc.
 */
template <unsigned int BITS>
class Hash {
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
  Hash() : storage_({}) {}

  Hash(const Hash &other) = default;

  Hash &operator=(const Hash &rhs) = default;

  Hash(Hash &&) noexcept = default;

  Hash &operator=(Hash &&rhs) noexcept = default;

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
    BLOCXXI_ASSERT_PRECOND(src_size <= Size());
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
  static Hash Max() noexcept {
    Hash h;
    h.storage_.fill(0xffffffffU);
    return h;
  }

  /// \brief Returns an all-0 hash, i.e. the smallest value representable by an
  /// N bit number (N/8 bytes).
  static Hash Min() noexcept {
    return Hash();
    // all bits are already 0
  }

  static Hash<BITS> FromHex(const std::string &src, bool reverse = false) {
    Hash<BITS> hash;
    codec::hex::Decode(gsl::make_span<>(src),
                       gsl::make_span<>(hash.Data(), hash.Size()), reverse);
    return hash;
  }

  static Hash<BITS> RandomHash() {
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
  reference At(size_type pos) {
    if (Size() <= pos) {
      throw std::out_of_range("Hash<BITS> index out of range");
    }
    return reinterpret_cast<pointer>(storage_.data())[pos];
  }
  const_reference At(size_type pos) const {
    if (Size() <= pos) {
      throw std::out_of_range("Hash<BITS> index out of range");
    }
    return reinterpret_cast<const_pointer>(storage_.data())[pos];
  };
  //@}

  /// @name Unchecked element access
  /// access specified element __without__ bounds checking.
  //@{
  /// Returns a reference to the element at specified location pos. No bounds
  /// checking is performed.
  reference operator[](size_type pos) noexcept {
    BLOCXXI_ASSERT_PRECOND(pos < Size());
    return reinterpret_cast<pointer>(storage_.data())[pos];
  }
  const_reference operator[](size_type pos) const noexcept {
    BLOCXXI_ASSERT_PRECOND(pos < Size());
    return reinterpret_cast<pointer>(storage_.data())[pos];
  }

  /// Returns a reference to the first element in the container.
  reference Front() { return reinterpret_cast<pointer>(storage_.data())[0]; }
  const_reference Front() const {
    return reinterpret_cast<const_pointer>(storage_.data())[0];
  }

  /// Returns reference to the last element in the container.
  reference Back() {
    return reinterpret_cast<pointer>(storage_.data())[Size() - 1];
  }
  const_reference Back() const {
    return reinterpret_cast<const_pointer>(storage_.data())[Size() - 1];
  }

  /// Returns pointer to the underlying array serving as element storage. The
  /// pointer is such that range [Data(); Data() + Size()) is always a valid
  /// range.
  pointer Data() noexcept { return reinterpret_cast<pointer>(storage_.data()); }
  const_pointer Data() const noexcept {
    return reinterpret_cast<const_pointer>(storage_.data());
  }
  //@}

  /// @name Iterators
  //@{
  /// Returns an iterator to the first element of the container.
  iterator begin() noexcept {
    return reinterpret_cast<pointer>(storage_.data());
  }
  const_iterator begin() const noexcept {
    return reinterpret_cast<const_pointer>(storage_.data());
  }
  const_iterator cbegin() const noexcept { return begin(); }

  /// Returns an iterator to the element following the last element of the
  /// container. This element acts as a placeholder; attempting to access it
  /// results in undefined behavior.
  iterator end() noexcept {
    return reinterpret_cast<pointer>(storage_.data()) + Size();
  }
  const_iterator end() const noexcept {
    return reinterpret_cast<const_pointer>(storage_.data()) + Size();
  }
  const_iterator cend() const noexcept { return end(); }

  /// Returns a reverse iterator to the first element of the reversed container.
  /// It corresponds to the last element of the non-reversed container.
  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(end());
  }
  const_reverse_iterator crbegin() const noexcept { return rbegin(); }

  /// Returns a reverse iterator to the element following the last element of
  /// the reversed container. It corresponds to the element preceding the first
  /// element of the non-reversed container. This element acts as a placeholder,
  /// attempting to access it results in undefined behavior.
  reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(begin());
  }
  const_reverse_iterator crend() const noexcept { return rend(); }
  //@}

  // Capacity

  /// The size of the hash in bytes.
  constexpr static std::size_t Size() { return BITS / 8; };

  /// The size of the hash in bytes.
  constexpr static std::size_t BitSize() { return BITS; };

  // Operations

  /// Set all bits to 0.
  void Clear() { storage_.fill(0); }

  /// Return true if all bits are set to 0.
  bool IsAllZero() const {
    for (auto v : storage_)
      if (v != 0) return false;
    return true;
  }

  /// Exchanges the contents of the container with those of other. Does not
  /// cause iterators and references to associate with the other container.
  void swap(Hash &other) noexcept { storage_.swap(other.storage_); }

  /// Count leading zero bits.
  int LeadingZeroBits() const {
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
  friend bool operator==(const Hash<BITS> &lhs, const Hash<BITS> &rhs) {
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
  }
  /// \brief Compares the contents of `lhs` and `rhs` lexicographically. The
  /// comparison is performed by a function equivalent to
  /// `std::lexicographical_compare`.
  friend bool operator<(const Hash<BITS> &lhs, const Hash<BITS> &rhs) {
    for (std::size_t i = 0; i < SIZE_DWORD; ++i) {
      std::uint32_t const native_lhs = detail::NetworkToHost(lhs.storage_[i]);
      std::uint32_t const native_rhs = detail::NetworkToHost(rhs.storage_[i]);
      if (native_lhs < native_rhs) return true;
      if (native_lhs > native_rhs) return false;
    }
    return false;
  }
  //@}

  /// In-place bitwise XOR with the given other hash.
  Hash &operator^=(Hash const &other) noexcept {
    for (std::size_t i = 0; i < SIZE_DWORD; ++i)
      storage_[i] ^= other.storage_[i];
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
    size_type dst_size = end() - start;
    BLOCXXI_ASSERT_PRECOND(src_size <= dst_size);
    std::memcpy(start, buf.data(), std::min(src_size, dst_size));
  }

  // TODO: Replace the parameters with a span
  void Randomize() { random::GenerateBlock(Data(), Size()); }

  const std::string ToHex() const {
    return codec::hex::Encode(gsl::make_span<>(Data(), Size()), false, true);
  }

  std::bitset<BITS> ToBitSet() const {
    std::bitset<BITS> bs;  // [0,0,...,0]
    int shift_left = BITS;
    for (uint32_t num_part : storage_) {
      shift_left -= 32;
      num_part = detail::HostToNetwork(num_part);
      std::bitset<BITS> bs_part(num_part);
      bs |= bs_part << shift_left;
    }
    return bs;
  }

  std::string ToBitStringShort(std::size_t length = 32U) const {
    auto truncate = false;
    if (length < BITS) { truncate = true; length -= 3; }
    auto str = ToBitSet().to_string().substr(0, length);
    if (truncate) str.append("...");
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
inline bool operator!=(const Hash<BITS> &lhs, const Hash<BITS> &rhs) {
  return !(lhs == rhs);
}
template <unsigned int BITS>
inline bool operator>(const Hash<BITS> &lhs, const Hash<BITS> &rhs) {
  return rhs < lhs;
}
template <unsigned int BITS>
inline bool operator<=(const Hash<BITS> &lhs, const Hash<BITS> &rhs) {
  return !(lhs > rhs);
}
template <unsigned int BITS>
inline bool operator>=(const Hash<BITS> &lhs, const Hash<BITS> &rhs) {
  return !(lhs < rhs);
}
//@}

/// Bitwise XOR
template <unsigned int BITS>
inline Hash<BITS> operator^(Hash<BITS> lhs, Hash<BITS> const &rhs) noexcept {
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
inline std::ostream &operator<<(std::ostream &out, Hash<BITS> const &hash) {
  out << hash.ToHex();
  return out;
}

using Hash512 = Hash<512>;  // 64 bytes
using Hash256 = Hash<256>;  // 32 bytes
using Hash160 = Hash<160>;  // 20 bytes

}  // namespace crypto
}  // namespace blocxxi

#endif  // BLOCXXI_CRYPTO_HASH_H_
