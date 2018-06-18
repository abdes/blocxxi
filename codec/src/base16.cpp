//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include "codec/base16.h"

#include <cctype>     // for isxdigit
#include <stdexcept>  // for std::runtime_exception

namespace blocxxi {
namespace codec {
namespace hex {

namespace {
/* Range generation,from
 * http://stackoverflow.com/questions/13313980/populate-an-array-using-constexpr-at-compile-time
 */
template<unsigned... Is>
struct seq {};
template<unsigned N, unsigned... Is>
struct gen_seq : gen_seq<N - 1, N - 1, Is...> {};
template<unsigned... Is>
struct gen_seq<0, Is...> : seq<Is...> {};

/** A table consisting of indexes and values,
 * which will all be computed at compile-time **/
template<unsigned N>
struct HexLookupTable {
  char lo_[N];
  char hi_[N];

  static constexpr unsigned length = N;
};

template<typename LambdaType, unsigned... Is>
constexpr HexLookupTable<sizeof...(Is)> HexTableGenerator(seq<Is...>,
                                                          LambdaType MakeLo,
                                                          LambdaType MakeHi) {
  return {{MakeLo(Is)...}, {MakeHi(Is)...}};
}

template<unsigned N, typename LambdaType>
constexpr HexLookupTable<N> HexTableGenerator(LambdaType MakeLo,
                                              LambdaType MakeHi) {
  return HexTableGenerator(gen_seq<N>(), MakeLo, MakeHi);
}

/** Function that computes a value for each index **/
constexpr const unsigned char ALPHABET_UC[]{"0123456789ABCDEF"};
constexpr char UpperMakeLo(unsigned idx) { return (ALPHABET_UC[idx & 0xFU]); }
constexpr char UpperMakeHi(unsigned idx) {
  return (ALPHABET_UC[(idx & 0xF0U) >> 4U]);
}

constexpr const unsigned char ALPHABET_LC[]{"0123456789abcdef"};
constexpr char LowerMakeLo(unsigned idx) { return (ALPHABET_LC[idx & 0xFU]); }
constexpr char LowerMakeHi(unsigned idx) {
  return (ALPHABET_LC[(idx & 0xF0U) >> 4U]);
}

// create compile-time table
HexLookupTable<256> HEX_LOOKUP_TABLE_UC =
    HexTableGenerator<256>(UpperMakeLo, UpperMakeHi);
HexLookupTable<256> HEX_LOOKUP_TABLE_LC =
    HexTableGenerator<256>(LowerMakeLo, LowerMakeHi);

/** A table consisting of indexes and values,
 * which will all be computed at compile-time **/
template<unsigned N>
struct DecLookupTable {
  uint8_t dec_[N];

  static constexpr unsigned length = N;
};

template<typename LambdaType, unsigned... Is>
constexpr DecLookupTable<sizeof...(Is)> DecTableGenerator(seq<Is...>,
                                                          LambdaType DecForIndex) {
#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif
  return {DecForIndex(Is)...};
#if __clang__
#pragma clang diagnostic pop
#endif
}

template<unsigned N, typename LambdaType>
constexpr DecLookupTable<N> DecTableGenerator(LambdaType DecForIndex) {
  return DecTableGenerator(gen_seq<N>(), DecForIndex);
}

constexpr uint8_t DecForIndex(uint8_t idx) {
  if ('0' <= idx && idx <= '9') {
    return idx - '0';
  } else if ('A' <= idx && idx <= 'F') {
    return idx + (uint8_t) 10 - 'A';
  } else if ('a' <= idx && idx <= 'f') {
    return idx + (uint8_t) 10 - 'a';
  } else {
    return 0xFF;  // Invalid value
  }
}

// create compile-time table
DecLookupTable<256> DEC_LOOKUP_TABLE = DecTableGenerator<256>(DecForIndex);

}  // namespace

std::string Encode(gsl::span<const uint8_t> src, bool reverse,
                   bool lower_case) {
  HexLookupTable<256> &table =
      (lower_case) ? HEX_LOOKUP_TABLE_LC : HEX_LOOKUP_TABLE_UC;

  std::string out;
  out.reserve(2 * src.size());
  for (auto bin : src) {
    out.push_back(table.hi_[bin]);
    out.push_back(table.lo_[bin]);
  }
  if (reverse) std::reverse(out.begin(), out.end());
  return out;
}

void Decode(gsl::span<const char> src, gsl::span<uint8_t> dest, bool reverse) {
  Expects(src.size() % 2 == 0);
  Expects(dest.size() >= src.size() / 2);

  // Do nothing if the source string is empty
  if (src.empty()) return;

  auto out = dest.begin();
  if (reverse) {
    auto src_begin = src.rbegin();
    auto src_end = src.rend();
    while (src_begin != src_end) {
      uint8_t lookup = DEC_LOOKUP_TABLE.dec_[static_cast<uint8_t>(*src_begin)];
      if (lookup == 0xFF)
        throw std::domain_error("Character ['" + std::to_string(*src_begin) +
            "'] not a valid hex digit");
      uint8_t dec = lookup << 4U;
      src_begin++;
      lookup = DEC_LOOKUP_TABLE.dec_[static_cast<uint8_t>(*src_begin)];
      if (lookup == 0xFF)
        throw std::domain_error("Character ['" + std::to_string(*src_begin) +
            "'] not a valid hex digit");
      dec |= lookup;
      src_begin++;
      *out++ = dec;
    }
  } else {
    auto src_begin = src.begin();
    auto src_end = src.end();
    while (src_begin != src_end) {
      uint8_t lookup = DEC_LOOKUP_TABLE.dec_[static_cast<uint8_t>(*src_begin)];
      if (lookup == 0xFF)
        throw std::domain_error("Character ['" + std::to_string(*src_begin) +
            "'] not a valid hex digit");
      uint8_t dec = lookup << 4U;
      src_begin++;
      lookup = DEC_LOOKUP_TABLE.dec_[static_cast<uint8_t>(*src_begin)];
      if (lookup == 0xFF)
        throw std::domain_error("Character ['" + std::to_string(*src_begin) +
            "'] not a valid hex digit");
      dec |= lookup;
      src_begin++;
      *out++ = dec;
    }
  }
  // Wipe the remaining of the destination span with '0'
  while (out != dest.end()) *out++ = 0;
}

}  // namespace hex
}  // namespace codec
}  // namespace blocxxi
