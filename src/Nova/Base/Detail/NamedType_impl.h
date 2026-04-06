//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

// Based on NamedType, Copyright (c) 2017 Jonathan Boccara
// License: MIT
// https://github.com/joboccara/NamedType

#pragma once

#include <initializer_list>
#include <tuple>
#include <type_traits>
#include <utility>

// MSVC requires __declspec(empty_bases) in some multiple-inheritance cases to
// guarantee empty base class optimization when non-empty members are present.
#ifdef _MSC_VER
#  define NOVA_EBCO __declspec(empty_bases)
#else
#  define NOVA_EBCO
#endif

namespace nova {

// Forward declaration: optional skill marker
template <typename> struct DefaultInitialized;

template <typename T>
using IsNotReference = std::enable_if_t<!std::is_reference_v<T>, void>;

namespace nt_detail {
  template <typename U> struct storage_no_init {
    U value_;
    constexpr storage_no_init() = default;
    template <typename... Args>
    explicit constexpr storage_no_init([[maybe_unused]] std::in_place_t tag,
      Args&&... args) noexcept(noexcept(U(std::forward<Args>(args)...)))
      : value_(std::forward<Args>(args)...)
    {
    }
  };
  template <typename U> struct storage_value_init {
    U value_ {};
    constexpr storage_value_init() = default;
    template <typename... Args>
    explicit constexpr storage_value_init([[maybe_unused]] std::in_place_t tag,
      Args&&... args) noexcept(noexcept(U(std::forward<Args>(args)...)))
      : value_(std::forward<Args>(args)...)
    {
    }
  };

  template <typename U> struct storage_no_init<U&> {
    U& value_;
    storage_no_init() = delete;
    explicit constexpr storage_no_init(
      [[maybe_unused]] std::in_place_t tag, U& u) noexcept
      : value_(u)
    {
    }
  };

} // namespace nt_detail

//! Strongly typed wrapper around an underlying type with opt-in skills.
/*!
 Provides a type-safe alias for an underlying type by pairing it with a unique
 tag parameter and composing optional skills. The skills are CRTP mixins that
 enable behaviors such as arithmetic, comparators, bitwise operators, hashing,
 printing, dereferencing, and callability without affecting unrelated strong
 types that share the same underlying type.

 The wrapper preserves triviality and size characteristics of the underlying
 type when possible (via empty-base optimization and storage selection). It
 forwards `constexpr` and `noexcept` semantics based on the underlying
 operations.

 Key capabilities:
 - Distinct strong types per `Parameter` tag, even for identical `T`.
 - Optional `ref` view type and lvalue-only conversion to avoid dangling.
 - Default-initialization control via the `DefaultInitialized` skill.
 - Named-arguments support through the nested `argument` helper.

 @tparam T         Underlying value type (may be a reference).
 @tparam Parameter Unique tag type that differentiates strong types.
 @tparam Skills    Zero or more skill mixins enabling behaviors for the wrapper

 ### Usage Examples

 ```cpp
 // Define strong types
 using Width  = myproject::NamedType<int, struct WidthTag>;
 using Height = myproject::NamedType<int, struct HeightTag>;

 // Compose arithmetic capability
 using Meters = myproject::NamedType<int, struct MetersTag, myproject::Addable>;

 // Construction and access
 Meters d{ 5 };
 int raw = d.get();

 // Addable via skill
 Meters e{ 7 };
 auto sum = d + e; // 12

 // Named arguments (order-independent with helper in this header)
 struct Person { Width w; Height h; };
 auto make = [](Width w, Height h) { return Person{ w, h }; };
 auto any_order = myproject::MakeNamedArgFunction<Width, Height>(make);
 Person p = any_order(Height{ 180 }, Width{ 75 });
 ```

 ```cpp
 // Strong reference:
 using FamilyNameRef =
   myproject::NamedType<std::string&, struct FamilyNameRefTag>;
 void set(FamilyNameRef n) { n.get() = "Doe"; }
 ```

 ```cpp
 // Composite skill example:
 using Meter = myproject::NamedType<int,
   struct MeterTag,
   myproject::Arithmetic>; // bundles many arithmetic/bitwise skills
 ```

 ```cpp
 // Named arguments with the nested `argument` helper:
 using First = myproject::NamedType<std::string, struct FirstTag>;
 using Last  = myproject::NamedType<std::string, struct LastTag>;
 static const First::argument first;
 static const Last::argument  last;
 auto display = [](First f, Last l) { ... };
 display(first = "John", last = "Doe");
 ```

 @note Conversion to `ref` is only allowed on non-const lvalues to avoid
 dangling references; rvalue conversion is disabled.
 @see myproject::DefaultInitialized, skills in `NamedType_skills.h`,
 myproject::MakeNamed, myproject::MakeNamedArgFunction
 */
template <typename T, typename Parameter, template <typename> class... Skills>
class NOVA_EBCO NamedType
  : private std::conditional_t<
      (!std::is_reference_v<T> && std::is_default_constructible_v<T>
        && (false || ...
          || std::is_same_v<
            DefaultInitialized<NamedType<T, Parameter, Skills...>>,
            Skills<NamedType<T, Parameter, Skills...>>>)),
      nt_detail::storage_value_init<T>, nt_detail::storage_no_init<T>>,
    public Skills<NamedType<T, Parameter, Skills...>>... {
public:
  using UnderlyingType = T;
  using Self = NamedType<T, Parameter, Skills...>;

  // default constructor follows base/member availability; stays trivial when
  // possible
  NamedType() = default;

  explicit constexpr NamedType(T const& value) noexcept(
    std::is_nothrow_copy_constructible_v<T>)
    : std::conditional_t<
        (!std::is_reference_v<T> && std::is_default_constructible_v<T>
          && (false || ...
            || std::is_same_v<
              DefaultInitialized<NamedType<T, Parameter, Skills...>>,
              Skills<NamedType<T, Parameter, Skills...>>>)),
        nt_detail::storage_value_init<T>, nt_detail::storage_no_init<T>>(
        std::in_place, value)
  {
  }

  template <typename T_ = T, typename = IsNotReference<T_>>
  explicit constexpr NamedType(T&& value) noexcept(
    std::is_nothrow_move_constructible_v<T>)
    : std::conditional_t<
        (!std::is_reference_v<T> && std::is_default_constructible_v<T>
          && (false || ...
            || std::is_same_v<
              DefaultInitialized<NamedType<T, Parameter, Skills...>>,
              Skills<NamedType<T, Parameter, Skills...>>>)),
        nt_detail::storage_value_init<T>, nt_detail::storage_no_init<T>>(
        std::in_place, std::move(value))
  {
  }

  // get
  [[nodiscard]] constexpr auto get() noexcept -> T& { return this->value_; }

  [[nodiscard]] constexpr auto get() const noexcept
    -> std::remove_reference_t<T> const&
  {
    return this->value_;
  }

  // conversions
  using ref = NamedType<T&, Parameter, Skills...>;
  // Only allow reference-view conversion on non-const lvalues to avoid dangling
  // NOLINTNEXTLINE(*-explicit-constructor)
  [[nodiscard]] constexpr operator ref() & noexcept
  {
    return ref(this->value_);
  }
  constexpr operator ref() && = delete; // prevent dangling from rvalues

  struct argument {
    // NOLINTNEXTLINE(*-assignment-signature)
    template <typename U> constexpr auto operator=(U&& value) const -> NamedType
    {
      return NamedType(std::forward<U>(value));
    }

    // Support braced-init for container-like Ts (e.g., std::vector)
    template <typename U = T,
      typename Elem = typename std::remove_reference_t<U>::value_type>
    // NOLINTNEXTLINE(*-assignment-signature)
    constexpr auto operator=(std::initializer_list<Elem> il) const -> NamedType
    {
      return NamedType(T { il });
    }

    argument() = default;
    ~argument() = default;
    argument(argument const&) = delete;
    argument(argument&&) = delete;
    auto operator=(argument const&) -> argument& = delete;
    auto operator=(argument&&) -> argument& = delete;
  };

  // Storage provided by base class selected via std::conditional_t above
};

//! Helper to construct a strong type from an underlying value.
/*!
 Constructs a strong type by applying an alias template `StrongType<T>` to the
 provided value. This is useful when the strong type is expressed as an alias
 template over the underlying type.

 @tparam StrongType Alias template of form `template<class> class` that maps
                    an underlying type `T` to a strong wrapper `StrongType<T>`.
 @tparam T          Underlying type of the value.
 @param  value      Underlying value to wrap.
 @return A `StrongType<T>` constructed from `value`.

 ### Usage Examples

 ```cpp
 // Generic strong type over a callable
 template <class F>
 using Comparator = myproject::NamedType<F, struct ComparatorTag>;

 template <class F>
 void performAction(Comparator<F> c) { c.get()(); }

 // Wrap a lambda into the strong Comparator type
 auto comp = myproject::MakeNamed<Comparator>([] { ...compare... });
 performAction(comp);
 ```

 ### Performance Characteristics

 - Time Complexity: O(1) (direct construction).
 - Memory: No additional allocation beyond the wrapper.

 @see myproject::NamedType
 */
template <template <typename T> class StrongType, typename T>
constexpr auto MakeNamed(T const& value) -> StrongType<T>
{
  return StrongType<T>(value);
}

namespace nt_detail {
  template <class F, class... Ts> struct AnyOrderCallable {
    F f;
    template <class... Us> auto operator()(Us&&... args) const
    {
      static_assert(
        sizeof...(Ts) == sizeof...(Us), "Passing wrong number of arguments");
      auto x = std::make_tuple(std::forward<Us>(args)...);
      return f(std::move(std::get<Ts>(x))...);
    }
  };
} // namespace nt_detail

// Utilities for named argument function

//! Build a callable that accepts named arguments in any order.
/*!
 Wraps a callable `f(Args...)` and returns an adapter that can be invoked with
 the same named arguments in any order. The adapter reorders its inputs to
 match `Args...` before forwarding them to `f`.

 Complements the `NamedType::argument` pattern by enabling order-independent
 passing at the call site without using assignment syntax.

 @tparam Args Strong/named types expected by `f` (the target parameter order).
 @tparam F    Callable type to wrap (copied by value into the adapter).
 @param  f    Callable to invoke with reordered arguments.
 @return A lightweight adapter callable that takes `Args...` in any order.

 ### Usage Examples

 ```cpp
 using Width  = myproject::NamedType<int, struct WidthTag>;
 using Height = myproject::NamedType<int, struct HeightTag>;

 auto make = [](Width w, Height h) { return std::pair{ w, h }; };
 auto any  = myproject::MakeNamedArgFunction<Width, Height>(make);
 auto pair = any(Height{ 180 }, Width{ 75 });
 ```

 ### Performance Characteristics

 - Time Complexity: O(1) for invocation and tuple reordering (compile-time
   index selection; runtime cost is minimal move/forward operations).
 - Memory: Stores a copy of `f`; no dynamic allocation.

 @warning The number of provided arguments must match `sizeof...(Args)`.
 @warning Each type in `Args...` must be unique; duplicates are ill-formed.
 @see myproject::NamedType, myproject::MakeNamed
 */
template <class... Args, class F> auto MakeNamedArgFunction(F&& f)
{
  return nt_detail::AnyOrderCallable<F, Args...> { std::forward<F>(f) };
}

} // namespace nova
