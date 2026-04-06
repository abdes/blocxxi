//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <tuple>
#include <type_traits>

/*!
@file ResourceTypeList.h

## Compile-Time Type List and Type Indexing Utilities

Defines the generic compile-time type list and type-indexing utilities for
general C++ metaprogramming. This file provides the `TypeList` and `IndexOf`
templates, which enable the creation of ordered type lists and compile-time
mapping from types to unique, stable, zero-based indices.

### Usage and Binary Compatibility Requirements

- Any set of types that require compile-time indexing or type-to-ID mapping
  should be listed in a single `TypeList` (e.g., `MyTypeList`).
- The order of types in the list determines their type index. **Never reorder
  existing types**; only append new types to the end to maintain binary
  compatibility for systems that depend on stable indices.
- Forward declare all types before defining the type list to avoid circular
  dependencies and enable use in headers.
- The type list must be visible to all code that needs to resolve type indices
  at compile time (e.g., registries, pools, handles, or other metaprogramming
  utilities).

### Example Usage

```cpp
// Forward declare all types
class Foo;
class Bar;
class Baz;

// Define the type list in a central header
using MyTypeList = myproject::TypeList<
    Foo,
    Bar,
    Baz,
>;

// Get a type index at compile time
constexpr std::size_t foo_index = myproject::IndexOf<Foo, MyTypeList>::value;
```

@warning Changing the order of types in the type list will break binary
         compatibility for all systems that depend on stable indices. Only
         append new types.
*/

namespace nova {

/*!
 Template metaprogramming-based resource type system. Provides compile-time
 unique resource type IDs without RTTI or runtime overhead.
*/
template <typename... Ts> struct TypeList { };

//! Template metaprogramming helper to find the index of a type in a TypeList.
/*!
 Recursively searches through the TypeList at compile time and returns the
 zero-based index where the type is found. Generates a compile error if the type
 is not found, providing type safety.

 @tparam T The type to search for
 @tparam List The TypeList to search in
*/
template <typename T, typename List> struct IndexOf;

template <typename T, typename... Ts>
struct IndexOf<T, TypeList<T, Ts...>> : std::integral_constant<std::size_t, 0> {
};

template <typename T, typename U, typename... Ts>
struct IndexOf<T, TypeList<U, Ts...>>
  : std::integral_constant<std::size_t,
      1 + IndexOf<T, TypeList<Ts...>>::value> { };

//! Template metaprogramming helper to compute the number of types in a
//! TypeList.
/*!
 @tparam List The TypeList to compute the size of.
 @return The number of types in the TypeList as std::size_t.

 ### Usage Example

 ```cpp
 using MyTypes = myproject::TypeList<int, float, double>;
 constexpr std::size_t count = myproject::TypeListSize<MyTypes>::value; // 3
 ```

 @see TypeList
*/
template <typename List> struct TypeListSize;

template <typename... Ts>
struct TypeListSize<TypeList<Ts...>>
  : std::integral_constant<std::size_t, sizeof...(Ts)> { };

/*!
 Template metaprogramming helper to apply a TypeList to a template.

 Transforms TypeList<A, B, C> into Template<A, B, C>, enabling the use of
 TypeList with other template metaprogramming utilities like std::tuple,
 std::variant, etc.

 @tparam Template The template to apply the TypeList to
 @tparam List The TypeList to apply

 ### Usage Example

 ```cpp
 using MyTypes = TypeList<int, float, double>;
 using MyTuple = Apply_t<std::tuple, MyTypes>; // std::tuple<int, float, double>
 using MyVariant = Apply_t<std::variant, MyTypes>; // std::variant<int, float,
 double>
 ```
*/
template <template <typename...> class Template, typename List> struct Apply;

template <template <typename...> class Template, typename... Types>
struct Apply<Template, TypeList<Types...>> {
  using Type = Template<Types...>;
};

//! Convenience alias for Apply<Template, List>::type
template <template <typename...> class Template, typename List>
using ApplyT = typename Apply<Template, List>::Type;

//! Transforms a TypeList<Ts...> into a new TypeList with a template applied to
//! each type.
/*!
 Applies the template `Template` to each type in the TypeList.

 @tparam List The TypeList to transform
 @tparam Template The template to apply (must be a template taking a single
 type)
 @return A std::tuple of the transformed types

 ### Usage Example

 ```cpp
 using MyTuple = TypeListTransform<MyTypeList, std::add_pointer>::type;
 ```

 @see TypeList
*/
template <typename List, template <typename> class Template>
struct TypeListTransform;

// Specialization for TypeList
template <template <typename...> class TypeList, typename... Ts,
  template <typename> class Template>
struct TypeListTransform<TypeList<Ts...>, Template> {
  using Type = std::tuple<Template<Ts>...>;
};

} // namespace nova
