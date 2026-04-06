//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

// Based on NamedType, Copyright (c) 2017 Jonathan Boccara
// License: MIT
// https://github.com/joboccara/NamedType

#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <Nova/Base/Crtp.h>
#include <Nova/Base/Detail/NamedType_impl.h>

namespace nova {

namespace nt_detail {
  template <typename...> using void_t = void;

  template <typename T, typename = void>
  struct is_ostream_insertable : std::false_type { };

  template <typename T>
  struct is_ostream_insertable<T,
    void_t<decltype(std::declval<std::ostream&>() << std::declval<T const&>())>>
    : std::true_type { };

  template <typename T>
  inline constexpr bool is_ostream_insertable_v
    = is_ostream_insertable<T>::value;

  template <typename T, typename = void>
  struct has_adl_to_string : std::false_type { };

  template <typename T>
  struct has_adl_to_string<T,
    void_t<decltype(to_string(std::declval<T const&>()))>> : std::true_type { };

  template <typename T>
  inline constexpr bool has_adl_to_string_v = has_adl_to_string<T>::value;

  template <typename T, typename = void>
  struct has_streamable_adl_to_string : std::false_type { };

  template <typename T>
  struct has_streamable_adl_to_string<T,
    void_t<decltype(to_string(std::declval<T const&>())),
      decltype(std::declval<std::ostream&>()
        << to_string(std::declval<T const&>()))>> : std::true_type { };

  template <typename T>
  inline constexpr bool has_streamable_adl_to_string_v
    = has_streamable_adl_to_string<T>::value;

  template <typename NT, typename = void>
  struct has_is_hashable : std::false_type { };

  template <typename NT>
  struct has_is_hashable<NT, void_t<decltype(NT::is_hashable)>>
    : std::true_type { };

  template <typename NT, bool = has_is_hashable<NT>::value>
  struct is_hashable_helper : std::false_type { };

  template <typename NT>
  struct is_hashable_helper<NT, true>
    : std::integral_constant<bool, static_cast<bool>(NT::is_hashable)> { };

  template <typename NT>
  inline constexpr bool is_hashable_v = is_hashable_helper<NT>::value;

  // Detect if a Skills pack contains a given skill template instantiation
  template <template <typename> class Skill, typename Named>
  struct has_skill : std::false_type { };

  template <template <typename> class Skill, typename T, typename Param,
    template <typename> class... Skills>
  struct has_skill<Skill, NamedType<T, Param, Skills...>>
    : std::disjunction<std::is_same<Skill<NamedType<T, Param, Skills...>>,
        Skills<NamedType<T, Param, Skills...>>>...> { };

  // primary, overridden via partial specialization later
  template <typename NT>
  inline constexpr bool has_default_initialized_v = false;

  template <typename> inline constexpr bool always_false_v = false;
} // namespace nt_detail

//! Provides prefix increment operator (++x) for NamedType
template <typename T>
struct NOVA_EBCO PreIncrementable : Crtp<T, PreIncrementable> {
  constexpr auto operator++() noexcept(noexcept(++this->underlying().get()))
    -> T&
  {
    ++this->underlying().get();
    return this->underlying();
  }
};

//! Provides postfix increment operator (x++) for NamedType
template <typename T>
struct NOVA_EBCO PostIncrementable : Crtp<T, PostIncrementable> {
  constexpr auto operator++(int) noexcept(noexcept(this->underlying().get()++))
    -> T
  {
    return T(this->underlying().get()++);
  }
};

//! Provides prefix decrement operator (--x) for NamedType
template <typename T>
struct NOVA_EBCO PreDecrementable : Crtp<T, PreDecrementable> {
  [[nodiscard]] constexpr auto operator--() noexcept(
    noexcept(--this->underlying().get())) -> T&
  {
    --this->underlying().get();
    return this->underlying();
  }
};

//! Provides postfix decrement operator (x--) for NamedType
template <typename T>
struct NOVA_EBCO PostDecrementable : Crtp<T, PostDecrementable> {
  [[nodiscard]] constexpr auto operator--(int) noexcept(
    noexcept(this->underlying().get()--)) -> T
  {
    return T(this->underlying().get()--);
  }
};

//! Provides binary addition operators (+, +=) for NamedType
template <typename T> struct NOVA_EBCO BinaryAddable : Crtp<T, BinaryAddable> {
  [[nodiscard]] constexpr auto operator+(T const& other) const
    noexcept(noexcept(this->underlying().get() + other.get())) -> T
  {
    return T(this->underlying().get() + other.get());
  }
  constexpr auto operator+=(T const& other) noexcept(
    noexcept(this->underlying().get() += other.get())) -> T&
  {
    this->underlying().get() += other.get();
    return this->underlying();
  }
};

//! Provides unary plus operator (+x) for NamedType
template <typename T> struct NOVA_EBCO UnaryAddable : Crtp<T, UnaryAddable> {
  [[nodiscard]] constexpr auto operator+() const
    noexcept(noexcept(+this->underlying().get())) -> T
  {
    return T(+this->underlying().get());
  }
};

//! Combines binary and unary addition operators for NamedType
template <typename T>
struct NOVA_EBCO Addable : BinaryAddable<T>, UnaryAddable<T> {
  using BinaryAddable<T>::operator+;
  using UnaryAddable<T>::operator+;
};

//! Provides binary subtraction operators (-, -=) for NamedType
template <typename T>
struct NOVA_EBCO BinarySubtractable : Crtp<T, BinarySubtractable> {
  [[nodiscard]] constexpr auto operator-(T const& other) const
    noexcept(noexcept(this->underlying().get() - other.get())) -> T
  {
    return T(this->underlying().get() - other.get());
  }
  constexpr auto operator-=(T const& other) noexcept(
    noexcept(this->underlying().get() -= other.get())) -> T&
  {
    this->underlying().get() -= other.get();
    return this->underlying();
  }
};

//! Provides unary minus operator (-x) for NamedType
template <typename T>
struct NOVA_EBCO UnarySubtractable : Crtp<T, UnarySubtractable> {
  [[nodiscard]] constexpr auto operator-() const
    noexcept(noexcept(-this->underlying().get())) -> T
  {
    return T(-this->underlying().get());
  }
};

//! Combines binary and unary subtraction operators for NamedType
template <typename T>
struct NOVA_EBCO Subtractable : BinarySubtractable<T>, UnarySubtractable<T> {
  using UnarySubtractable<T>::operator-;
  using BinarySubtractable<T>::operator-;
};

//! Provides multiplication operators (*, *=) for NamedType
template <typename T> struct NOVA_EBCO Multiplicable : Crtp<T, Multiplicable> {
  [[nodiscard]] constexpr auto operator*(T const& other) const
    noexcept(noexcept(this->underlying().get() * other.get())) -> T
  {
    return T(this->underlying().get() * other.get());
  }
  constexpr auto operator*=(T const& other) noexcept(
    noexcept(this->underlying().get() *= other.get())) -> T&
  {
    this->underlying().get() *= other.get();
    return this->underlying();
  }
};

//! Provides division operators (/, /=) for NamedType
template <typename T> struct NOVA_EBCO Divisible : Crtp<T, Divisible> {
  [[nodiscard]] constexpr auto operator/(T const& other) const
    noexcept(noexcept(this->underlying().get() / other.get())) -> T
  {
    return T(this->underlying().get() / other.get());
  }
  constexpr auto operator/=(T const& other) noexcept(
    noexcept(this->underlying().get() /= other.get())) -> T&
  {
    this->underlying().get() /= other.get();
    return this->underlying();
  }
};

//! Provides modulo operators (%, %=) for NamedType
template <typename T> struct NOVA_EBCO Modulable : Crtp<T, Modulable> {
  [[nodiscard]] constexpr auto operator%(T const& other) const
    noexcept(noexcept(this->underlying().get() % other.get())) -> T
  {
    return T(this->underlying().get() % other.get());
  }
  constexpr auto operator%=(T const& other) noexcept(
    noexcept(this->underlying().get() %= other.get())) -> T&
  {
    this->underlying().get() %= other.get();
    return this->underlying();
  }
};

//! Provides bitwise inversion operator (~) for NamedType
template <typename T>
struct NOVA_EBCO BitWiseInvertable : Crtp<T, BitWiseInvertable> {
  [[nodiscard]] constexpr auto operator~() const
    noexcept(noexcept(~this->underlying().get())) -> T
  {
    return T(~this->underlying().get());
  }
};

//! Provides bitwise AND operators (&, &=) for NamedType
template <typename T>
struct NOVA_EBCO BitWiseAndable : Crtp<T, BitWiseAndable> {
  [[nodiscard]] constexpr auto operator&(T const& other) const
    noexcept(noexcept(this->underlying().get() & other.get())) -> T
  {
    return T(this->underlying().get() & other.get());
  }
  constexpr auto operator&=(T const& other) noexcept(
    noexcept(this->underlying().get() &= other.get())) -> T&
  {
    this->underlying().get() &= other.get();
    return this->underlying();
  }
};

//! Provides bitwise OR operators (|, |=) for NamedType
template <typename T> struct NOVA_EBCO BitWiseOrable : Crtp<T, BitWiseOrable> {
  [[nodiscard]] constexpr auto operator|(T const& other) const
    noexcept(noexcept(this->underlying().get() | other.get())) -> T
  {
    return T(this->underlying().get() | other.get());
  }
  constexpr auto operator|=(T const& other) noexcept(
    noexcept(this->underlying().get() |= other.get())) -> T&
  {
    this->underlying().get() |= other.get();
    return this->underlying();
  }
};

//! Provides bitwise XOR operators (^, ^=) for NamedType
template <typename T>
struct NOVA_EBCO BitWiseXorable : Crtp<T, BitWiseXorable> {
  [[nodiscard]] constexpr auto operator^(T const& other) const
    noexcept(noexcept(this->underlying().get() ^ other.get())) -> T
  {
    return T(this->underlying().get() ^ other.get());
  }
  constexpr auto operator^=(T const& other) noexcept(
    noexcept(this->underlying().get() ^= other.get())) -> T&
  {
    this->underlying().get() ^= other.get();
    return this->underlying();
  }
};

//! Provides bitwise left shift operators (<<, <<=) for NamedType
template <typename T>
struct NOVA_EBCO BitWiseLeftShiftable : Crtp<T, BitWiseLeftShiftable> {
  [[nodiscard]] constexpr auto operator<<(T const& other) const
    noexcept(noexcept(this->underlying().get() << other.get())) -> T
  {
    return T(this->underlying().get() << other.get());
  }
  constexpr auto operator<<=(T const& other) noexcept(
    noexcept(this->underlying().get() <<= other.get())) -> T&
  {
    this->underlying().get() <<= other.get();
    return this->underlying();
  }
};

//! Provides bitwise right shift operators (>>, >>=) for NamedType
template <typename T>
struct NOVA_EBCO BitWiseRightShiftable : Crtp<T, BitWiseRightShiftable> {
  [[nodiscard]] constexpr auto operator>>(T const& other) const
    noexcept(noexcept(this->underlying().get() >> other.get())) -> T
  {
    return T(this->underlying().get() >> other.get());
  }
  constexpr auto operator>>=(T const& other) noexcept(
    noexcept(this->underlying().get() >>= other.get())) -> T&
  {
    this->underlying().get() >>= other.get();
    return this->underlying();
  }
};

//! Provides comparison operators via spaceship operator for NamedType
template <typename T> struct NOVA_EBCO Comparable : Crtp<T, Comparable> {
  [[nodiscard]] constexpr auto operator<=>(Comparable<T> const& other) const
    noexcept(noexcept(this->underlying().get() <=> other.underlying().get()))
  {
    return this->underlying().get() <=> other.underlying().get();
  }

  [[nodiscard]] constexpr auto operator==(Comparable<T> const& other) const
    noexcept(noexcept(this->underlying().get() == other.underlying().get()))
      -> bool
  {
    return this->underlying().get() == other.underlying().get();
  }
};

//! Provides dereference operator (*) for NamedType
template <typename T> struct NOVA_EBCO Dereferencable;

template <typename T, typename Parameter, template <typename> class... Skills>
struct Dereferencable<NamedType<T, Parameter, Skills...>>
  : Crtp<NamedType<T, Parameter, Skills...>, Dereferencable> {
  [[nodiscard]] constexpr auto operator*() & noexcept -> T&
  {
    return this->underlying().get();
  }
  [[nodiscard]] constexpr auto operator*() const& noexcept
    -> std::remove_reference_t<T> const&
  {
    return this->underlying().get();
  }
};

//! Provides implicit conversion capability to specified destination type
template <typename Destination> struct NOVA_EBCO ImplicitlyConvertibleTo {
  template <typename T> struct templ : Crtp<T, templ> {
    // NOLINTNEXTLINE(google-explicit-constructor)
    [[nodiscard]] constexpr operator Destination() const
      noexcept(noexcept(static_cast<Destination>(this->underlying().get())))
    {
      return this->underlying().get();
    }
  };
};

//! Provides stream output capability for NamedType
template <typename T> struct NOVA_EBCO Printable : Crtp<T, Printable> {
  static constexpr bool is_printable = true;

  void print(std::ostream& os) const
  {
    using UnderlyingT = std::remove_cv_t<
      std::remove_reference_t<decltype(this->underlying().get())>>;

    if constexpr (nt_detail::is_ostream_insertable_v<UnderlyingT>) {
      os << this->underlying().get();
    } else if constexpr (nt_detail::has_streamable_adl_to_string_v<T>) {
      os << to_string(this->underlying());
    } else {
      os << "<unprintable>";
    }
  }
};

template <typename T, typename Parameter, template <typename> class... Skills>
  requires NamedType<T, Parameter, Skills...>::is_printable
auto operator<<(std::ostream& os,
  NamedType<T, Parameter, Skills...> const& object) -> std::ostream&
{
  object.print(os);
  return os;
}

//! Enables std::hash support for NamedType
template <typename T> struct NOVA_EBCO Hashable {
  static constexpr bool is_hashable = true;
};

//! Provides function call conversion operators for NamedType
template <typename NamedType_> struct NOVA_EBCO FunctionCallable;

template <typename T, typename Parameter, template <typename> class... Skills>
struct FunctionCallable<NamedType<T, Parameter, Skills...>>
  : Crtp<NamedType<T, Parameter, Skills...>, FunctionCallable> {
  // NOLINTNEXTLINE(google-explicit-constructor)
  [[nodiscard]] constexpr operator T const&() const noexcept
  {
    return this->underlying().get();
  }
  // NOLINTNEXTLINE(google-explicit-constructor)
  [[nodiscard]] constexpr operator T&() noexcept
  {
    return this->underlying().get();
  }
};

//! Provides member access operator (->) for NamedType
template <typename NamedType_> struct NOVA_EBCO MethodCallable;

template <typename T, typename Parameter, template <typename> class... Skills>
struct MethodCallable<NamedType<T, Parameter, Skills...>>
  : Crtp<NamedType<T, Parameter, Skills...>, MethodCallable> {
  [[nodiscard]] constexpr auto operator->() const noexcept
    -> std::remove_reference_t<T> const*
  {
    return std::addressof(this->underlying().get());
  }
  [[nodiscard]] constexpr auto operator->() noexcept
    -> std::remove_reference_t<T>*
  {
    return std::addressof(this->underlying().get());
  }
};

//! Combines function and method call capabilities for NamedType
template <typename NamedType_>
struct NOVA_EBCO Callable : FunctionCallable<NamedType_>,
                            MethodCallable<NamedType_> { };

//! Combines prefix and postfix increment operators for NamedType
template <typename T>
struct NOVA_EBCO Incrementable : PreIncrementable<T>, PostIncrementable<T> {
  using PostIncrementable<T>::operator++;
  using PreIncrementable<T>::operator++;
};

//! Combines prefix and postfix decrement operators for NamedType
template <typename T>
struct NOVA_EBCO Decrementable : PreDecrementable<T>, PostDecrementable<T> {
  using PostDecrementable<T>::operator--;
  using PreDecrementable<T>::operator--;
};

//! Comprehensive arithmetic operations skill combining all arithmetic operators
/*!
 Provides a complete set of arithmetic operations for NamedType including:
 increment, decrement, addition, subtraction, multiplication, division,
 modulo, bitwise operations, comparison operators, printing, and hashing.

 @see Incrementable, Decrementable, Addable, Subtractable, Multiplicable
 @see Divisible, Modulable, Comparable, Printable, Hashable
*/
template <typename T>
struct NOVA_EBCO Arithmetic : Incrementable<T>,
                              Decrementable<T>,
                              Addable<T>,
                              Subtractable<T>,
                              Multiplicable<T>,
                              Divisible<T>,
                              Modulable<T>,
                              BitWiseInvertable<T>,
                              BitWiseAndable<T>,
                              BitWiseOrable<T>,
                              BitWiseXorable<T>,
                              BitWiseLeftShiftable<T>,
                              BitWiseRightShiftable<T>,
                              Comparable<T>,
                              Printable<T>,
                              Hashable<T> { };

//! Enables default initialization for NamedType instances
template <typename T>
struct NOVA_EBCO DefaultInitialized : Crtp<T, DefaultInitialized> {
  static constexpr bool default_initialized = true;
};

} // namespace nova

namespace std {
template <typename T, typename Parameter, template <typename> class... Skills>
struct hash<nova::NamedType<T, Parameter, Skills...>> {
  using NamedType = nova::NamedType<T, Parameter, Skills...>;
  auto operator()(
    nova::NamedType<T, Parameter, Skills...> const& x) const noexcept -> size_t
  {
    // Only usable when Hashable skill opted in
    static_assert(nova::nt_detail::is_hashable_v<NamedType>,
      "nova::NamedType is not hashable: add nova::Hashable");
    static_assert(
      noexcept(std::hash<T>()(x.get())), "hash function should not throw");

    return std::hash<T>()(x.get());
  }
};
} // namespace std

namespace fmt {
template <typename T, typename Parameter, template <typename> class... Skills>
struct formatter<nova::NamedType<T, Parameter, Skills...>, char> {
  using NamedType = nova::NamedType<T, Parameter, Skills...>;
  using UnderlyingType = std::remove_cv_t<std::remove_reference_t<T>>;

  constexpr auto parse(format_parse_context& ctx)
    -> format_parse_context::iterator
  {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const NamedType& value, FormatContext& ctx) const ->
    typename FormatContext::iterator
  {
    if constexpr (nova::nt_detail::has_skill<nova::Printable,
                    NamedType>::value) {
      return fmt::format_to(ctx.out(), "{}", fmt::streamed(value));
    } else if constexpr (fmt::is_formattable<UnderlyingType, char>::value) {
      return fmt::formatter<UnderlyingType, char> {}.format(value.get(), ctx);
    } else {
      static_assert(nova::nt_detail::always_false_v<NamedType>,
        "NamedType is not formattable: add nova::Printable skill or make the "
        "underlying type fmt-formattable");
      return ctx.out();
    }
  }
};
} // namespace fmt
