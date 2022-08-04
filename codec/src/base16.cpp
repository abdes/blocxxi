//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

/*!
 * \file
 *
 * \brief Implementation of the encoding/decoding utilities for blocxxi,
 */

#include <array>
#include <codec/base16.h>

#include <cctype> // for isxdigit
#include <cstdint>
#include <stdexcept> // for std::runtime_exception

namespace blocxxi::codec::hex {

namespace {
/* Range generation,from
 * http://stackoverflow.com/questions/13313980/populate-an-array-using-constexpr-at-compile-time
 */
template <unsigned... Is> struct seq {};
template <unsigned N, unsigned... Is>
struct gen_seq : gen_seq<N - 1, N - 1, Is...> {};
template <unsigned... Is> struct gen_seq<0, Is...> : seq<Is...> {};

/** A table consisting of indexes and values,
 * which will all be computed at compile-time **/
template <unsigned N> struct HexLookupTable {
  std::array<char, N> lo_;
  std::array<char, N> hi_;

  static constexpr unsigned length = N;
};

template <typename LambdaType, unsigned... Is>
constexpr auto HexTableGenerator(seq<Is...> /*unused*/, LambdaType MakeLo,
    LambdaType MakeHi) -> HexLookupTable<sizeof...(Is)> {
  return {{MakeLo(Is)...}, {MakeHi(Is)...}};
}

template <unsigned N, typename LambdaType>
constexpr auto HexTableGenerator(LambdaType MakeLo, LambdaType MakeHi)
    -> HexLookupTable<N> {
  return HexTableGenerator(gen_seq<N>(), MakeLo, MakeHi);
}

constexpr uint8_t c_lower_four_bits_mask = 0xFU;
constexpr uint8_t c_upper_four_bits_mask = 0xF0U;

/** Function that computes a value for each index **/
constexpr std::array ALPHABET_UC{'0', '1', '2', '3', '4', '5', '6', '7', '8',
    '9', 'A', 'B', 'C', 'D', 'E', 'F'};
constexpr auto UpperMakeLo(unsigned idx) -> char {
  return (ALPHABET_UC.at(idx & c_lower_four_bits_mask));
}
constexpr auto UpperMakeHi(unsigned idx) -> char {
  return (ALPHABET_UC.at((idx & c_upper_four_bits_mask) >> 4));
}

constexpr std::array ALPHABET_LC{'0', '1', '2', '3', '4', '5', '6', '7', '8',
    '9', 'a', 'b', 'c', 'd', 'e', 'f'};
constexpr auto LowerMakeLo(unsigned idx) -> char {
  return (ALPHABET_LC.at(idx & c_lower_four_bits_mask));
}
constexpr auto LowerMakeHi(unsigned idx) -> char {
  return (ALPHABET_LC.at((idx & c_upper_four_bits_mask) >> 4));
}

// create compile-time table
const HexLookupTable<256> HEX_LOOKUP_TABLE_UC =
    HexTableGenerator<256>(UpperMakeLo, UpperMakeHi);
const HexLookupTable<256> HEX_LOOKUP_TABLE_LC =
    HexTableGenerator<256>(LowerMakeLo, LowerMakeHi);

/** A table consisting of indexes and values,
 * which will all be computed at compile-time **/
template <unsigned N> struct DecLookupTable {
  std::array<uint8_t, N> dec_;

  static constexpr unsigned length = N;
};

template <typename LambdaType, unsigned... Is>
constexpr auto DecTableGenerator(seq<Is...> /*unused*/, LambdaType DecForIndex)
    -> DecLookupTable<sizeof...(Is)> {
#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif
  return {DecForIndex(Is)...};
#if __clang__
#pragma clang diagnostic pop
#endif
}

template <unsigned N, typename LambdaType>
constexpr auto DecTableGenerator(LambdaType DecForIndex) -> DecLookupTable<N> {
  return DecTableGenerator(gen_seq<N>(), DecForIndex);
}

constexpr uint8_t c_invalid_value = 0xFF;

constexpr auto DecForIndex(uint8_t idx) -> uint8_t {
  constexpr uint8_t c_value_of_a = 10;
  uint8_t decodec_value = c_invalid_value;
  if ('0' <= idx && idx <= '9') {
    decodec_value = idx - '0';
  } else if ('A' <= idx && idx <= 'F') {
    decodec_value = idx + c_value_of_a - 'A';
  } else if ('a' <= idx && idx <= 'f') {
    decodec_value = idx + c_value_of_a - 'a';
  } else {
    // Keep the invalid value used to initialize the decoded_value variable
  }
  return decodec_value;
}

// create compile-time table
constexpr size_t c_lookup_table_size = 256;
const DecLookupTable<c_lookup_table_size> DEC_LOOKUP_TABLE =
    DecTableGenerator<c_lookup_table_size>(DecForIndex);

} // namespace

auto Encode(gsl::span<const uint8_t> src, bool reverse, bool lower_case)
    -> std::string {
  const HexLookupTable<c_lookup_table_size> &table =
      (lower_case) ? HEX_LOOKUP_TABLE_LC : HEX_LOOKUP_TABLE_UC;

  std::string out;
  out.reserve(2 * src.size());
  for (auto bin : src) {
    out.push_back(table.hi_.at(bin));
    out.push_back(table.lo_.at(bin));
  }
  if (reverse) {
    std::reverse(out.begin(), out.end());
  }
  return out;
}

void Decode(gsl::span<const char> src, gsl::span<uint8_t> dest, bool reverse) {
  Expects(src.size() % 2 == 0);
  Expects(dest.size() >= src.size() / 2);

  // Do nothing if the source string is empty
  if (src.empty()) {
    return;
  }

  auto out = dest.begin();
  if (reverse) {
    auto src_begin = src.rbegin();
    auto src_end = src.rend();
    while (src_begin != src_end) {
      uint8_t lookup =
          DEC_LOOKUP_TABLE.dec_.at(static_cast<uint8_t>(*src_begin));
      if (lookup == c_invalid_value) {
        throw std::domain_error("Character ['" + std::to_string(*src_begin) +
                                "'] not a valid hex digit");
      }
      uint8_t dec = lookup << 4U;
      src_begin++;
      lookup = DEC_LOOKUP_TABLE.dec_.at(static_cast<uint8_t>(*src_begin));
      if (lookup == c_invalid_value) {
        throw std::domain_error("Character ['" + std::to_string(*src_begin) +
                                "'] not a valid hex digit");
      }
      dec |= lookup;
      src_begin++;
      *out++ = dec;
    }
  } else {
    auto src_begin = src.begin();
    auto src_end = src.end();
    while (src_begin != src_end) {
      uint8_t lookup =
          DEC_LOOKUP_TABLE.dec_.at(static_cast<uint8_t>(*src_begin));
      if (lookup == c_invalid_value) {
        throw std::domain_error("Character ['" + std::to_string(*src_begin) +
                                "'] not a valid hex digit");
      }
      uint8_t dec = lookup << 4U;
      src_begin++;
      lookup = DEC_LOOKUP_TABLE.dec_.at(static_cast<uint8_t>(*src_begin));
      if (lookup == c_invalid_value) {
        throw std::domain_error("Character ['" + std::to_string(*src_begin) +
                                "'] not a valid hex digit");
      }
      dec |= lookup;
      src_begin++;
      *out++ = dec;
    }
  }
  // Wipe the remaining of the destination span with '0'
  while (out != dest.end()) {
    *out++ = 0;
  }
}

} // namespace blocxxi::codec::hex
