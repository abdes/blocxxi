//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

/*!
 * \file
 *
 * \brief Base16 encoding/decoding routines.
 */

#pragma once

#include <codec/blocxxi_codec_api.h>

#include <cstdint> // for uint8_t
#include <gsl/gsl> // for gsl::span
#include <string>  // for std::string

namespace blocxxi::codec::hex {

/*!
 * \brief Encode buffer to an hexadecimal/base16 string, as per RFC-4648. The
 * result is an hexadecimal/base16 encoded UTF-8 string.
 *
 * \param [in] src data to be encoded.
 * \param [in] reverse when `true`, the encoding will start from the end of the
 * buffer instead of the beginning. Default is `false`.
 * \param [in] lower_case when `true`, the encoded string will use lower case
 * letters for the hex digits. Default is `false`.
 * \return A string containing the hex-encoded data.
 */
BLOCXXI_CODEC_API auto Encode(gsl::span<const uint8_t> src,
    bool reverse = false, bool lower_case = false) -> std::string;

/*!
 * \brief Decode an hexadecimal/base16 encoded string, as per RFC-4648. Input
 * data is assumed to be an hexadecimal/base16 encoded UTF-8 string.
 *
 * \param [in] src buffer containing encoded data.
 * \param [out] dest buffer to receive the decoded data. Must be large enough to
 * receive all of the decoded data.
 * \param [in] reverse when `true`, the decoding will start from the end of the
 * buffer instead of the beginning. Default is `false`.
 *
 * \throw std::domain_error if the input data contains characters not part of
 * the valid alphabet for Base 16 encoding.
 *
 * \note Providing a `dest` buffers that is smaller than what is needed will
 * abort the program. It is the responsibility of the caller to ensure the
 * provided range is enough for the input data.
 */
BLOCXXI_CODEC_API void Decode(
    gsl::span<const char> src, gsl::span<uint8_t> dest, bool reverse = false);

} // namespace blocxxi::codec::hex
