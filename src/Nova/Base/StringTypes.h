//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

/*!
 @file StringUtils.h
 @brief Contains utilities for string manipulation.

 Windows-specific functions are fenced with `NOVA_WINDOWS` and are only
 available on the Windows platform.
*/

#include <string>

namespace nova::string_utils {

//! Any form of a string, UTF-8 string or wide string r-value object.
template <typename T>
concept StringType = std::convertible_to<T, std::string_view>
  || std::convertible_to<T, std::u8string_view>
  || std::convertible_to<T, std::wstring_view>;

//! Any form of literal string, literal UTF-8 string or literal wide string
template <typename T>
concept LiteralStringType = requires(T t) {
  { std::string(t) } -> std::convertible_to<std::string>;
  { std::wstring(t) } -> std::convertible_to<std::wstring>;
  { std::u8string(t) } -> std::convertible_to<std::u8string>;
};

//! Any form of string, literal or not.
template <typename T>
concept AnyStringType = StringType<T> || LiteralStringType<T>;

} // namespace nova::string_utils
