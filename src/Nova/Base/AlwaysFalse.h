//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

namespace nova {

//! Helper for `static_assert` to fail dependent on template parameter.
/*!
 This template is used when you need to create a compile-time assertion that
 depends on a template parameter. Since it always evaluates to false regardless
 of the template parameter, it can be used in contexts where you need to fail
 compilation for specific template instantiations.

 <b>Example usage:</b>
 ```cpp
 template <typename T>
 void process(const T& value) {
     if constexpr (std::is_integral_v<T>) {
         // Handle integer types
     } else if constexpr (std::is_floating_point_v<T>) {
         // Handle floating point types
     } else {
         static_assert(always_false_v<T>, "Unsupported type!");
     }
 }
 ```
*/
template <typename> // ReSharper disable once CppInconsistentNaming
inline constexpr bool always_false_v = false;

} // namespace nova
