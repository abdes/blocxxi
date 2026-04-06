//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace nostd {

namespace adl_helper {

  //! ADL helper: bring std::to_string into scope and perform unqualified call
  /*!
    This helper centralizes the ADL lookup used by `nostd::to_string`.
    It places `using std::to_string;` into the helper's scope so that an
    unqualified `to_string(x)` performs argument-dependent lookup (ADL).

    The function template `as_string` is intentionally declared with a
    trailing-return-type using `decltype(to_string(std::declval<T>()))` so
    that the expression is evaluated in an unevaluated context. This makes
    `adl_helper::as_string` and therefore `nostd::to_string` SFINAE-friendly:
    when no suitable `to_string` overload is found for a type `T`, the
    substitution fails instead of producing a hard error.
  */

  using std::to_string;

  template <class T>
  auto as_string(T&& value) -> decltype(to_string(std::declval<T>()))
  {
    return to_string(std::forward<T>(value));
  }

  //! ADL helper: bring std::as_bytes into scope and perform unqualified call.
  /*!
    This enables `nostd::as_bytes(x)` to resolve either:
    - custom ADL
   * overloads `as_bytes(x)` defined in `x`'s namespace, or
    -
   * `std::as_bytes(span)` when called with spans.
  */
  using std::as_bytes;

  template <class T>
  auto as_read_only_bytes(T&& value)
    -> decltype(as_bytes(std::forward<T>(value)))
  {
    return as_bytes(std::forward<T>(value));
  }

  //! ADL helper: bring std::as_writable_bytes into scope and unqualified call.
  /*!
    This enables `nostd::as_writable_bytes(x)` to resolve either:
    -
   * custom ADL overloads `as_writable_bytes(x)` in `x`'s namespace, or
    -
   * `std::as_writable_bytes(span)` when called with writable spans.
  */
  using std::as_writable_bytes;

  template <class T>
  auto as_mutable_bytes(T&& value)
    -> decltype(as_writable_bytes(std::forward<T>(value)))
  {
    return as_writable_bytes(std::forward<T>(value));
  }

} // namespace adl_helper

//! Converts an enum to its underlying type.
/*!
 * This is a C++23 feature that we backport here for C++20 compatibility.
 */
template <typename Enum> constexpr auto to_underlying(Enum e) noexcept
{
  return static_cast<std::underlying_type_t<Enum>>(e);
}

//! Project-wide ADL entrypoint: nostd::to_string
/*!
  `nostd::to_string` centralizes ADL-based conversions for logging and other
  boundary code.

  Behavior:
  - It forwards to `adl_helper::as_string`, which performs an unqualified
    `to_string(x)` lookup with `using std::to_string;` in scope so ADL can
    find user-defined `to_string` overloads in the argument's namespace.
  - The implementation uses a trailing-return-type (`decltype(...)`) so that
    `nostd::to_string` participates in SFINAE: when no `to_string` is
    available for a type `T`, the function template is removed from overload
    resolution instead of emitting a hard compile error. This property is
    relied upon by adapter code that detects ADL availability via concepts
    or `decltype`-based probes.

  Example:
  ```cpp
  namespace foo {
    struct Bar { int v; };
    std::string to_string(const Bar& b) { return "Bar(" + std::to_string(b.v) +
  ")"; }
  }

  auto s = nostd::to_string(foo::Bar{42}); // "Bar(42)"
  ```
*/
template <class T>
auto to_string(T&& value)
  -> decltype(adl_helper::as_string(std::forward<T>(value)))
{
  return adl_helper::as_string(std::forward<T>(value));
}

//! Project-wide ADL entrypoint: nostd::as_bytes
/*!
  Forwards to ADL-visible `as_bytes(x)` and falls back to
 * `std::as_bytes(span)`
  when appropriate.
*/
template <class T>
auto as_bytes(T&& value)
  -> decltype(adl_helper::as_read_only_bytes(std::forward<T>(value)))
{
  return adl_helper::as_read_only_bytes(std::forward<T>(value));
}

//! Project-wide ADL entrypoint: nostd::as_writable_bytes
/*!
  Forwards to ADL-visible `as_writable_bytes(x)` and falls back to

 * `std::as_writable_bytes(span)` when appropriate.
*/
template <class T>
auto as_writable_bytes(T&& value)
  -> decltype(adl_helper::as_mutable_bytes(std::forward<T>(value)))
{
  return adl_helper::as_mutable_bytes(std::forward<T>(value));
}

//! Concept: does nostd::to_string(T) return something convertible to
//! std::string
template <typename T>
concept HasToStringReturnsString = requires(T&& t) {
  { nostd::to_string(std::forward<T>(t)) } -> std::convertible_to<std::string>;
};

//! Concept: does nostd::to_string(T) return something convertible to
//! std::string_view
template <typename T>
concept HasToStringReturnsStringView = requires(T&& t) {
  {
    nostd::to_string(std::forward<T>(t))
  } -> std::convertible_to<std::string_view>;
};

//! Concept: whether `nostd::to_string(t)` is available and returns something
//! convertible to `std::string` or `std::string_view`.
template <typename T>
concept HasToString
  = HasToStringReturnsString<T> || HasToStringReturnsStringView<T>;

} // namespace nostd
