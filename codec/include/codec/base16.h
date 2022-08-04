//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <codec/blocxxi_codec_api.h>

#include <cstdint> // for uint8_t
#include <gsl/gsl> // for gsl::span
#include <string>  // for std::string

namespace blocxxi::codec::hex {

BLOCXXI_CODEC_API auto Encode(gsl::span<const uint8_t> src,
    bool reverse = false, bool lower_case = false) -> std::string;

BLOCXXI_CODEC_API void Decode(
    gsl::span<const char> src, gsl::span<uint8_t> dest, bool reverse = false);

} // namespace blocxxi::codec::hex
