//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

// NOLINTBEGIN(*-macro-usage)

// use this inside a class declaration to make it non-copyable
#define NOVA_MAKE_NON_COPYABLE(Type)                                           \
  Type(const Type&) = delete;                                                  \
  auto operator=(const Type&)->Type& = delete;

// use this inside a class declaration to make it non-moveable
// NOLINTBEGIN
#define NOVA_MAKE_NON_MOVABLE(Type)                                            \
  Type(Type&&) = delete;                                                       \
  auto operator=(Type&&)->Type& = delete;
// NOLINTEND

// use this inside a class declaration to declare default copy constructor and
// assignment operator
#define NOVA_DEFAULT_COPYABLE(Type)                                            \
  Type(const Type&) = default;                                                 \
  auto operator=(const Type&)->Type& = default;

// use this inside a class declaration to declare default move constructor and
// move assignment operator
// NOLINTBEGIN
#define NOVA_DEFAULT_MOVABLE(Type)                                             \
  Type(Type&&) = default;                                                      \
  auto operator=(Type&&)->Type& = default;
// NOLINTEND

// Make a bit flag
#define NOVA_FLAG(x) (1 << (x))

// NOLINTBEGIN(*-macro-parentheses)
#define NOVA_DEFINE_FLAGS_OPERATORS(EnumType)                                  \
  inline constexpr EnumType operator|(EnumType lhs, EnumType rhs)              \
  {                                                                            \
    return static_cast<EnumType>(                                              \
      static_cast<std::underlying_type_t<EnumType>>(lhs)                       \
      | static_cast<std::underlying_type_t<EnumType>>(rhs));                   \
  }                                                                            \
  inline constexpr EnumType& operator|=(EnumType& lhs, EnumType rhs)           \
  {                                                                            \
    lhs = lhs | rhs;                                                           \
    return lhs;                                                                \
  }                                                                            \
  inline constexpr EnumType operator&(EnumType lhs, EnumType rhs)              \
  {                                                                            \
    return static_cast<EnumType>(                                              \
      static_cast<std::underlying_type_t<EnumType>>(lhs)                       \
      & static_cast<std::underlying_type_t<EnumType>>(rhs));                   \
  }                                                                            \
  inline constexpr EnumType& operator&=(EnumType& lhs, EnumType rhs)           \
  {                                                                            \
    lhs = lhs & rhs;                                                           \
    return lhs;                                                                \
  }                                                                            \
  inline constexpr EnumType operator^(EnumType lhs, EnumType rhs)              \
  {                                                                            \
    return static_cast<EnumType>(                                              \
      static_cast<std::underlying_type_t<EnumType>>(lhs)                       \
      ^ static_cast<std::underlying_type_t<EnumType>>(rhs));                   \
  }                                                                            \
  inline constexpr EnumType& operator^=(EnumType& lhs, EnumType rhs)           \
  {                                                                            \
    lhs = lhs ^ rhs;                                                           \
    return lhs;                                                                \
  }                                                                            \
  inline constexpr EnumType operator~(EnumType flag)                           \
  {                                                                            \
    return static_cast<EnumType>(                                              \
      ~static_cast<std::underlying_type_t<EnumType>>(flag));                   \
  }
  // NOLINTEND(*-macro-parentheses)

// NOLINTEND(*-macro-usage)
