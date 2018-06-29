//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <cstdint>   // for uint8_t
#include <gsl/gsl>  // for gsl::span
#include <string>    // for std::string

namespace blocxxi {
namespace codec {
namespace hex {

std::string Encode(gsl::span<const uint8_t> src, bool reverse = false,
                   bool lower_case = false);

void Decode(gsl::span<const char> src, gsl::span<uint8_t> dest,
            bool reverse = false);

}  // namespace hex
}  // namespace codec
}  // namespace blocxxi
