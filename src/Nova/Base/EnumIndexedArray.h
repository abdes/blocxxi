//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <array>
#include <concepts>
#include <cstdlib>
#include <ranges>
#include <type_traits>

#include <compare>
#include <stdexcept>

#include <Nova/Base/Macros.h>
#include <Nova/Base/NoStd.h>

namespace nova {

//! Concept: EnumWithCount
/*!
 Constrains enums used with helpers in this header. An enum satisfying
 `EnumWithCount` must expose `kFirst` and `kCount` enumerators where `kFirst`
 has underlying value `0` and `kCount` is greater than zero. This enables dense
 indexing and iteration across the enum range.

 @tparam E The enum type being tested.
*/
template<typename E>
concept EnumWithCount = std::is_enum_v<E> // must be an enum
  // Must have a count value, and it must be > 0
  && requires { { E::kCount } -> std::same_as<E>; }
  && (nostd::to_underlying(E::kCount) > 0)
  // must have a kFirst symbolic value, with underlying `0`
  && requires { { E::kFirst } -> std::same_as<E>; }
  && (nostd::to_underlying(E::kFirst) == 0);

//! EnumAsIndex: strongly-typed enum index wrapper
/*!
 A compact, strongly-typed index wrapper for enums that satisfy `EnumWithCount`.
 Use `EnumAsIndex<Enum>` to hold a numeric index derived from an enum while
 preserving type-safety and providing checked operations. The class supports
 constexpr usage, runtime-checked construction, and a consteval overload for
 compile-time validation.

 ### Usage Example

 ```cpp
 for (auto idx = EnumAsIndex<MyEnum>::Checked(MyEnum::kFirst);
      idx < EnumAsIndex<MyEnum>::end(); ++idx) {
   auto e = idx.to_enum();
 }
 ```

 @tparam Enum Enum type satisfying `EnumWithCount`.
*/
template <EnumWithCount Enum> class EnumAsIndex {
public:
  // Underlying integral type of the enum and the raw index type used to store
  // numeric indices. Keep these aliases to reduce repetitive casts and make
  // future migration easier.
  using RawIndexType = std::size_t;

  // Only allow construction from Enum
  constexpr explicit EnumAsIndex(Enum id) noexcept
    : value_(static_cast<RawIndexType>(id))
  {
    if (!std::is_constant_evaluated()) {
      if (value_ >= static_cast<RawIndexType>(Enum::kCount)) {
        std::terminate();
      }
    }
  }

  // Constexpr runtime-checked overload (keeps previous runtime behaviour). This
  // is selected by existing call sites that pass an Enum at runtime.
  [[nodiscard]] constexpr static auto Checked(Enum id) noexcept -> EnumAsIndex
  {
    return EnumAsIndex(id);
  }

  //! Consteval template overload for compile-time use. Call as
  //! `EnumAsIndex<MyEnum>::Checked<MyEnum::kFirst>()` to get a compile-time
  //! validated index without invoking the runtime-checking public ctor.
  template <Enum V> consteval static auto Checked() -> EnumAsIndex
  {
    static_assert(nostd::to_underlying(V) < nostd::to_underlying(Enum::kCount),
      "EnumAsIndex out of range");
    return EnumAsIndex(
      static_cast<RawIndexType>(nostd::to_underlying(V)), RawUncheckedTag {});
  }

  // Deleted default and raw-size constructors enforce invariants.
  constexpr EnumAsIndex() = delete;
  constexpr EnumAsIndex(RawIndexType) = delete;

  // Access underlying numeric index.
  [[nodiscard]] constexpr auto get() const noexcept { return value_; }

  [[nodiscard]] constexpr auto to_enum() const noexcept
  {
    return static_cast<Enum>(value_);
  }

  //! True when this index refers to a valid enum value (not the End()
  //! sentinel).
  [[nodiscard]] constexpr auto IsValid() const noexcept
  {
    return value_ < static_cast<RawIndexType>(Enum::kCount);
  }

  static constexpr auto begin() noexcept -> EnumAsIndex
  {
    return EnumAsIndex(
      static_cast<std::size_t>(Enum::kFirst), RawUncheckedTag {});
  }

  //! One-past-end sentinel to support idiomatic loops: for (EnumAsIndex<E>
  //! p{E::kFirst}; p < EnumAsIndex<E>::End(); ++p)
  static constexpr auto end() noexcept -> EnumAsIndex
  {
    return EnumAsIndex(
      static_cast<std::size_t>(Enum::kCount), RawUncheckedTag {});
  }

  // Explicit three-way compare (portable across toolchains).
  constexpr auto operator<=>(EnumAsIndex other) const noexcept
    -> std::strong_ordering
  {
    return value_ <=> other.value_;
  }

  // Pre-increment
  constexpr auto operator++() noexcept -> EnumAsIndex&
  {
    value_ = CheckedFromValueAndOffset(value_, 1);
    return *this;
  }

  // Post-increment
  constexpr auto operator++(int) noexcept -> EnumAsIndex
  {
    EnumAsIndex tmp = *this;
    ++(*this);
    return tmp;
  }

  // Pre-decrement
  constexpr auto operator--() noexcept -> EnumAsIndex&
  {
    value_ = CheckedFromValueAndOffset(value_, -1);
    return *this;
  }

  // Post-decrement
  constexpr auto operator--(int) noexcept -> EnumAsIndex
  {
    EnumAsIndex tmp = *this;
    --(*this);
    return tmp;
  }

  constexpr auto operator+=(std::ptrdiff_t off) noexcept -> auto&
  {
    value_ = CheckedFromValueAndOffset(value_, off);
    return *this;
  }

  constexpr auto operator-=(std::ptrdiff_t off) noexcept -> auto&
  {
    value_ = CheckedFromValueAndOffset(value_, -off);
    return *this;
  }

  [[nodiscard]] friend constexpr auto operator+(
    EnumAsIndex p, std::ptrdiff_t off) noexcept
  {
    return EnumAsIndex(
      CheckedFromValueAndOffset(p.get(), off), RawUncheckedTag {});
  }

  [[nodiscard]] friend constexpr auto operator-(
    EnumAsIndex p, std::ptrdiff_t off) noexcept
  {
    return EnumAsIndex(
      CheckedFromValueAndOffset(p.get(), -off), RawUncheckedTag {});
  }

  //! For algorithms that need 'how many steps apart'; enables
  //! `std::ranges::distance`-style usage.
  [[nodiscard]] friend constexpr auto operator-(
    EnumAsIndex a, EnumAsIndex b) noexcept
  {
    return static_cast<std::ptrdiff_t>(a.value_)
      - static_cast<std::ptrdiff_t>(b.value_);
  }

private:
  struct RawUncheckedTag { };

  RawIndexType value_;

  // Private constructor from raw value to allow creating the one-past-end
  // sentinel (value == Enum::kCount).
  constexpr explicit EnumAsIndex(std::size_t raw, RawUncheckedTag tag) noexcept
    : value_(raw)
  {
    (void)tag;
  }

  // Helper to validate a raw signed value and cast to the RawIndexType. This
  // centralizes the consteval vs runtime handling so we don't duplicate the
  // same branches in every operator implementation.
  static constexpr auto CheckedFromValueAndOffset(
    RawIndexType value, std::ptrdiff_t off) noexcept -> RawIndexType
  {
    const auto raw = static_cast<std::ptrdiff_t>(value) + off;
    if (!std::is_constant_evaluated()) {
      if (raw < 0 || raw > static_cast<std::ptrdiff_t>(Enum::kCount)) {
        std::terminate();
      }
    }
    return static_cast<RawIndexType>(raw);
  }
};

//! Class template deduction guide: allow `EnumAsIndex{MyEnum::kFirst}` to
//! deduce the template parameter and prefer the enum-based constructor.
template <EnumWithCount E> EnumAsIndex(E) -> EnumAsIndex<E>;

//! Namespace-visible equality for EnumAsIndex templates. Placed at namespace
//! scope so ADL/lookup finds it in contexts where std::equal_to is used.
template <EnumWithCount E>
constexpr auto operator==(
  EnumAsIndex<E> const& a, EnumAsIndex<E> const& b) noexcept -> bool
{
  return a.get() == b.get();
}

} // namespace nova

// Implement a full iterator + view that yields `EnumAsIndex<Enum>` values.
namespace nova {

//! EnumAsIndexIterator
/*!
 Iterator adapter that yields `EnumAsIndex<Enum>` values. Models a random-access
 iterator and is compatible with `EnumAsIndexView`. Use this iterator when
 iterating enum ranges in algorithms or range-based for loops.

 @tparam Enum Enum type satisfying `EnumWithCount`.
*/
template <EnumWithCount Enum> class EnumAsIndexIterator {
public:
  using value_type = EnumAsIndex<Enum>;
  using difference_type = std::ptrdiff_t;
  using reference = value_type; // dereference returns prvalue wrapper
  using pointer = void;
  using iterator_category = std::random_access_iterator_tag;
  using iterator_concept = std::random_access_iterator_tag;

  constexpr EnumAsIndexIterator() noexcept = default;
  ~EnumAsIndexIterator() = default;
  NOVA_DEFAULT_COPYABLE(EnumAsIndexIterator)
  NOVA_DEFAULT_MOVABLE(EnumAsIndexIterator)
  constexpr explicit EnumAsIndexIterator(std::size_t raw) noexcept
    : raw_(raw)
  {
  }

  constexpr auto operator*() const noexcept -> reference
  {
    return EnumAsIndex<Enum>::Checked(static_cast<Enum>(raw_));
  }

  constexpr auto operator++() noexcept -> EnumAsIndexIterator&
  {
    ++raw_;
    return *this;
  }
  constexpr auto operator++(int) noexcept -> EnumAsIndexIterator
  {
    auto tmp = *this;
    ++*this;
    return tmp;
  }
  constexpr auto operator--() noexcept -> EnumAsIndexIterator&
  {
    --raw_;
    return *this;
  }
  constexpr auto operator--(int) noexcept -> EnumAsIndexIterator
  {
    auto tmp = *this;
    --*this;
    return tmp;
  }

  constexpr auto operator+=(difference_type off) noexcept
    -> EnumAsIndexIterator&
  {
    raw_ = static_cast<std::size_t>(static_cast<difference_type>(raw_) + off);
    return *this;
  }
  constexpr auto operator-=(difference_type off) noexcept
    -> EnumAsIndexIterator&
  {
    raw_ = static_cast<std::size_t>(static_cast<difference_type>(raw_) - off);
    return *this;
  }

  friend constexpr auto operator+(
    EnumAsIndexIterator it, difference_type off) noexcept -> EnumAsIndexIterator
  {
    it += off;
    return it;
  }
  friend constexpr auto operator-(
    EnumAsIndexIterator it, difference_type off) noexcept -> EnumAsIndexIterator
  {
    it -= off;
    return it;
  }
  friend constexpr auto operator-(
    EnumAsIndexIterator a, EnumAsIndexIterator b) noexcept -> difference_type
  {
    return static_cast<difference_type>(a.raw_)
      - static_cast<difference_type>(b.raw_);
  }

  constexpr auto operator<=>(EnumAsIndexIterator other) const noexcept
    -> std::strong_ordering
  {
    return raw_ <=> other.raw_;
  }
  friend constexpr auto operator==(
    EnumAsIndexIterator a, EnumAsIndexIterator b) noexcept -> bool
  {
    return a.raw_ == b.raw_;
  }

private:
  std::size_t raw_ = 0;
};

//! EnumAsIndexView
/*!
 Range view that yields `EnumAsIndex<Enum>` across the enum's valid values.
 Prefer `enum_as_index<Enum>` for a convenient constexpr instance usable in
 range-based loops and algorithms.

 ### Example Usage:

 ```cpp
   for (auto idx : enum_as_index<MyEnum>) {
     // idx is EnumAsIndex<MyEnum>
   }
     ```

 @tparam Enum Enum type satisfying `EnumWithCount`.
*/
template <EnumWithCount Enum>
class EnumAsIndexView
  : public std::ranges::view_interface<EnumAsIndexView<Enum>> {
public:
  using iterator = EnumAsIndexIterator<Enum>;
  using sentinel = iterator;

  constexpr EnumAsIndexView() = default;
  ~EnumAsIndexView() = default;
  NOVA_DEFAULT_COPYABLE(EnumAsIndexView)
  NOVA_DEFAULT_MOVABLE(EnumAsIndexView)
  constexpr auto begin() noexcept -> iterator
  {
    return iterator(static_cast<std::size_t>(Enum::kFirst));
  }
  constexpr auto end() noexcept -> iterator
  {
    return iterator(static_cast<std::size_t>(Enum::kCount));
  }
  constexpr auto begin() const noexcept -> iterator
  {
    return iterator(static_cast<std::size_t>(Enum::kFirst));
  }
  constexpr auto end() const noexcept -> iterator
  {
    return iterator(static_cast<std::size_t>(Enum::kCount));
  }
  [[nodiscard]] constexpr auto size() const noexcept -> std::size_t
  {
    return static_cast<std::size_t>(Enum::kCount);
  }
  [[nodiscard]] constexpr auto empty() const noexcept -> bool
  {
    return size() == 0;
  }
};

} // namespace nova

namespace std::ranges {
template <nova::EnumWithCount E>
inline constexpr bool enable_view<nova::EnumAsIndexView<E>> = true;

template <nova::EnumWithCount E>
inline constexpr bool enable_borrowed_range<nova::EnumAsIndexView<E>> = true;
}

//! `enum_as_index` convenience variable
/*!
 Constexpr view instance that yields `EnumAsIndex<Enum>`. Use in range-based
 loops to iterate enum values safely and readably.
*/
template <nova::EnumWithCount Enum>
inline constexpr auto enum_as_index = nova::EnumAsIndexView<Enum> {};

//! Provide std::hash specialization for EnumAsIndex so it can be used in hashed
//! containers. This is a template so any EnumAsIndex<Enum> is hashable.
template <typename Enum> struct std::hash<nova::EnumAsIndex<Enum>> {
  constexpr auto operator()(nova::EnumAsIndex<Enum> idx) const noexcept
    -> std::size_t
  {
    return static_cast<std::size_t>(idx.get());
  }
}; // namespace std

namespace nova {

//! EnumIndexedArray
/*!
 Lightweight wrapper around `std::array` that lets callers index using an enum
 type (or a dedicated index wrapper) without manual casts. The array size is
 derived from `Enum::kCount` via the `enum_as_index` view to avoid mismatches
 and keep declarations concise.

 Key Features
 - Strongly-typed enum indexing without casts.
 - `at()` overloads that perform range checks and throw `std::out_of_range`.
 - Overloads supporting `Enum`, `EnumAsIndex<Enum>`, numeric indices, and any
   named index type exposing a `get()` returning `std::size_t`.

 Usage Example
 ```cpp
 enum class MyEnum { kFirst = 0, A = 0, B = 1, kCount = 2 };
 nova::EnumIndexedArray<MyEnum, int> arr{};
 arr[MyEnum::A] = 10;
 auto v = arr.at(MyEnum::B);
 ```

 Performance Characteristics
 - Time Complexity: O(1) for index and `at()` access (checked).
 - Memory: same as underlying `std::array<T, N>`.
 - Optimization: Avoids casts and improves readability; `operator[]` overloads
   are noexcept and suitable for hot paths.

 @tparam Enum Enum type satisfying `EnumWithCount`.
 @tparam T Value type stored in the array.
*/
template <EnumWithCount Enum, typename T> struct EnumIndexedArray {
  // Derive array size from the enum's kCount to avoid redundant template
  // parameters and accidental mismatches. Prefer deriving the size from the
  // enum_as_index view so the array size always matches the view's size.
  // std::ranges::size on the view is constexpr and maps directly to
  // Enum::kCount, but using the view makes the relationship explicit and less
  // error-prone.
  std::array<T, std::ranges::size(enum_as_index<Enum>)> data;

  // Index with enum (use underlying_type for portability)
  constexpr auto operator[](Enum e) noexcept -> T&
  {
    return data[static_cast<std::size_t>(nostd::to_underlying(e))];
  }
  constexpr auto operator[](Enum e) const noexcept -> T const&
  {
    return data[static_cast<std::size_t>(nostd::to_underlying(e))];
  }

  // Index with numeric index
  constexpr auto operator[](std::size_t i) noexcept -> T& { return data[i]; }
  constexpr auto operator[](std::size_t i) const noexcept -> T const&
  {
    return data[i];
  }

  // Checked accessors that match std::array::at(): throw std::out_of_range on
  // invalid index. Provide overloads for numeric indices, enum values and the
  // EnumAsIndex wrapper as well as named-index types exposing `get()`.
  constexpr auto at(std::size_t i) -> T&
  {
    if (i >= data.size()) {
      throw std::out_of_range("EnumIndexedArray::at: index out of range");
    }
    return data[i];
  }

  constexpr auto at(std::size_t i) const -> T const&
  {
    if (i >= data.size()) {
      throw std::out_of_range("EnumIndexedArray::at: index out of range");
    }
    return data[i];
  }

  constexpr auto at(Enum e) -> T&
  {
    return at(static_cast<std::size_t>(nostd::to_underlying(e)));
  }

  constexpr auto at(Enum e) const -> T const&
  {
    return at(static_cast<std::size_t>(nostd::to_underlying(e)));
  }

  constexpr auto at(EnumAsIndex<Enum> idx) -> T&
  {
    auto i = idx.get();
    if (i >= data.size()) {
      throw std::out_of_range("EnumIndexedArray::at: index out of range");
    }
    return data[i];
  }

  constexpr auto at(EnumAsIndex<Enum> idx) const -> T const&
  {
    auto i = idx.get();
    if (i >= data.size()) {
      throw std::out_of_range("EnumIndexedArray::at: index out of range");
    }
    return data[i];
  }

  template <typename NamedIndex>
  constexpr auto at(NamedIndex idx) -> T&
    requires requires(NamedIndex n) {
      { n.get() } -> std::convertible_to<std::size_t>;
    }
  {
    return at(idx.get());
  }

  template <typename NamedIndex>
  constexpr auto at(NamedIndex idx) const -> T const&
    requires requires(NamedIndex n) {
      { n.get() } -> std::convertible_to<std::size_t>;
    }
  {
    return at(idx.get());
  }

  // Support a named index wrapper with `get()` accessor (see PhaseIndex below)
  template <typename NamedIndex>
  constexpr auto operator[](NamedIndex idx) noexcept -> T&
  {
    return data[idx.get()];
  }
  template <typename NamedIndex>
  constexpr auto operator[](NamedIndex idx) const noexcept -> T const&
  {
    return data[idx.get()];
  }

  // Explicit overload for the enum-index wrapper (EnumAsIndex<Enum>) with a
  // debug assertion to prevent accidental indexing using the End() sentinel.
  // This is a no-op in release builds but helps catch sentinel misuse during
  // development. This overload is generic for any EnumAsIndex matching the
  // array's Enum template parameter.
  constexpr auto operator[](EnumAsIndex<Enum> idx) noexcept -> T&
  {
    const auto i = idx.get();
    if (i >= data.size()) {
      std::abort();
    }
    return data[i];
  }
  constexpr auto operator[](EnumAsIndex<Enum> idx) const noexcept -> T const&
  {
    const auto i = idx.get();
    if (i >= data.size()) {
      std::abort();
    }
    return data[i];
  }

  constexpr auto size() const noexcept { return data.size(); }
  constexpr auto begin() noexcept { return data.begin(); }
  constexpr auto end() noexcept { return data.end(); }
  constexpr auto begin() const noexcept { return data.begin(); }
  constexpr auto end() const noexcept { return data.end(); }
};

} // namespace nova
