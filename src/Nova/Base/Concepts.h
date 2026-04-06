//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <concepts>
#include <type_traits>

namespace nova {

//! Concept to check if a type is a pointer to a specific target type, ignoring
//! cv-qualifiers on the pointed-to type.
/*!
 This concept validates that:
 1. The type is a pointer type
 2. The pointed-to type (ignoring const/volatile) matches the expected type

 @tparam Pointer The type to check (should be a pointer type)
 @tparam Expected The expected pointed-to type (without cv-qualifiers)

 Examples:
 ```cpp
 static_assert(PointerTo<int*, int>);                    // true
 static_assert(PointerTo<const int*, int>);              // true
 static_assert(PointerTo<volatile int*, int>);           // true
 static_assert(PointerTo<const volatile int*, int>);     // true
 static_assert(PointerTo<int, int>);                     // false (not a
 pointer) static_assert(PointerTo<int*, float>);                  // false
 (wrong type)
 ```
*/
template <typename Pointer, typename Expected>
concept PointerTo = std::is_pointer_v<Pointer>
  && std::same_as<std::remove_cv_t<std::remove_pointer_t<Pointer>>, Expected>;

//! Concept to check if a type is a const pointer to a specific target type.
template <typename Pointer, typename Expected>
concept ConstPointerTo = PointerTo<Pointer, Expected>
  && std::is_const_v<std::remove_pointer_t<Pointer>>;

//! Concept to check if a type is a mutable (non-const) pointer to a specific
//! target type.
template <typename Pointer, typename Expected>
concept MutablePointerTo = PointerTo<Pointer, Expected>
  && !std::is_const_v<std::remove_pointer_t<Pointer>>;

} // namespace nova
