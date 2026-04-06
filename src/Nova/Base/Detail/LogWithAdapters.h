//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

//! Small logging adapter helpers used by Loguru when fmt is enabled.
/*!
 @file LogWithAdapters.h

 This header provides a lightweight adapter layer which maps objects that
 provide an ADL `to_string(T)` (accessed via the `nostd::to_string`) or are
 string-like into `std::string` before forwarding the arguments to
 `fmt::format`. Placing these mappings here keeps core headers free of
 fmt-related formatter specializations and confines formatting glue to the
 logging boundary.

 Usage: only included by logging code under `#if LOGURU_USE_FMTLIB`. Note:
 `nostd::to_string` is expected to be SFINAE-friendly so the adapter can detect
 availability via a concept; see `has_nostd_to_string` below.

  ### ADL example

  The adapters rely on ADL-visible free `to_string(T)` helpers. Example:

  ```cpp
  namespace foo {
    struct Bar { int v; };

    // ADL-visible free function in same namespace as `Bar`.
    std::string to_string(const Bar& b)
    {
      return "Bar(" + std::to_string(b.v) + ")";
    }
  }

  // Elsewhere, `nostd::to_string` will find the ADL free function above.
  #include <Nova/Base/NoStd.h>
  auto s = nostd::to_string(foo::Bar{42}); // "Bar(42)"

  // The logging boundary will use the same conversion so LOG_F works:
  LOG_F(INFO, "Value: {}", foo::Bar{42});
  ```
*/

#if LOGURU_USE_FMTLIB

#  include <concepts>
#  include <fmt/format.h>
#  include <string>
#  include <string_view>
#  include <tuple>
#  include <type_traits>
#  include <utility>

#  include <Nova/Base/NoStd.h>

namespace oxgn_lg_detail {

//! Detect string-like types
/*!
 A type is considered "string-like" if it is convertible to `std::string_view`.
 Such types are converted to `std::string` by the mapping layer to provide a
 stable, owning buffer for fmt calls.
*/
template <typename T>
concept StringLike = std::convertible_to<T, std::string_view>;

//! Detect whether fmt has a formatter for T
template <typename T>
concept Formattable
  = fmt::is_formattable<std::remove_reference_t<T>, fmt::format_context>::value;

//! Convert argument via ADL `to_string` when available
/*!
 @tparam T Type of the argument.
 @param x The value to map.
 @return An owning `std::string` produced by `nostd::to_string(x)`.

 When an ADL-visible `to_string(T)` exists, this overload materializes the
 result into a `std::string`. The conversion delegates to `nostd::to_string` to
 maintain centralized ADL behavior from `NoStd.h`. The implementation selects a
 safe materialization strategy based on the return type and value category to
 avoid ambiguous overload resolution while ensuring rvalue-based views are
 captured.
*/
template <typename T>
  requires(nostd::HasToString<std::remove_reference_t<T>> && !Formattable<T>)
auto MapArg(T&& x) -> std::string
{
  using Decayed = std::remove_reference_t<T>;
  if constexpr (nostd::HasToStringReturnsString<Decayed>) {
    // to_string returns std::string (or convertible): just return it.
    return nostd::to_string(std::forward<T>(x));
  } else if constexpr (nostd::HasToStringReturnsStringView<Decayed>) {
    // to_string returns string_view: materialize a local copy/moved value so
    // any returned view points into storage we control. We intentionally
    // materialize unconditionally to avoid lifetime pitfalls stemming from
    // unexpected evaluation order or forwarding contexts.
    Decayed local = std::forward<T>(x);
    return std::string(nostd::to_string(local));
  } else {
    // Fallback: should not be reachable because of requires; do NOT throw
    // (logging must be noexcept) — terminate to indicate a programming error.
    std::terminate();
  }
}

//! Convert string-like types to owning std::string
/*!
 For types convertible to `std::string_view`, create an owning `std::string` so
 subsequent formatting operations have stable storage.
*/
template <typename T>
  requires(!nostd::HasToString<T> && StringLike<T> && !Formattable<T>)
auto MapArg(T const& x) -> std::string
{
  return std::string(x);
}

// Special-case C-style strings: handle nullptr safely.
//! Safe mapping for C-style strings
/*!
 Converts `nullptr` to the string "(null)" and otherwise copies the pointed-to
 characters into an owning `std::string`.
*/
inline auto MapArg(const char* x) -> std::string
{
  if (x == nullptr) {
    return { "(null)" };
  }
  return { x };
}

//! Prefer fmt if it knows how to format the type. Return by value so
//! temporaries are owned by the tuple created in WithMappedArgs and don't
//! dangle.
template <typename T>
  requires(Formattable<T>)
auto MapArg(T&& x) -> std::remove_reference_t<T>
{
  return std::forward<T>(x);
}

//! Fallback: passthrough for types fmt can format directly
/*!
 For types that neither provide ADL `to_string` nor are string-like, we forward
 the value. This allows fmt to format scalars and user types that have
 registered formatters. Returning by value ensures temporaries are safely
 owned by the tuple created in WithMappedArgs and don't dangle.
*/
template <typename T>
  requires(!nostd::HasToString<T> && !StringLike<T> && !Formattable<T>)
auto MapArg(T const& x) -> T
{
  return x;
}

//! Materialize mapped arguments and invoke callable while storage is live
/*!
 @tparam F Callable type invoked with the mapped arguments.
 @tparam Args Argument types to map.
 @param f Callable to invoke. Called with lvalue references to the materialized
           mapped arguments.
 @param args Arguments forwarded into the mapping layer.
 @return Whatever `f` returns.

 This helper centralizes the delicate lifetime rules required when using
 `fmt::make_format_args`. `fmt::make_format_args` needs lvalue references to
 argument objects that remain valid when the `format_args` are created.
 `WithMappedArgs` materializes mapped values (for example, `std::string` results
 from ADL `to_string`) into a tuple that lives for the duration of the call. It
 then invokes the provided callable inside a `std::apply` with lvalue references
 to those mapped objects.

 Use this function at the logging boundary to ensure temporaries produced by
 mapping live long enough for `fmt::make_format_args` to safely reference them.

  ### Usage Example

  ```cpp
  // Materialize mapped args and call fmt safely from the callable.
  WithMappedArgs(
    [&](auto&... ms) {
      vlog(verbosity, file, line, fmt::make_format_args(ms...));
    },
    value1, value2);
  ```
*/
template <typename F, typename... Args>
auto WithMappedArgs(F&& f, Args&&... args) -> decltype(auto)
{
  auto m = std::make_tuple(oxgn_lg_detail::MapArg(std::forward<Args>(args))...);
  return std::apply(
    [&](auto&... ms) -> decltype(auto) { return std::forward<F>(f)(ms...); },
    m);
}

} // namespace oxgn_lg_detail

//! Format a string using the mapping adapters
/*!
 @param fmt_str The fmt-style (brace-based) format string passed via
 `fmt::runtime` (null-terminated).
 @param args Arguments to be mapped then forwarded to `fmt::format`.
 @return A `std::string` containing the formatted result.

 This convenience wraps `WithMappedArgs` and calls `fmt::format` from inside the
 callable. It's the safe, owning-path used by `textprintf` where a concrete
 `std::string` must be returned.

  ### Usage Example

  ```cpp
  // Format into an owning string using the adapters. Safe for temporaries.
  auto s = FormatWithAdapters("{} - {}", some_named_type(), "ok");
  ```
*/
template <typename... Args>
auto FormatWithAdapters(const char* fmt_str, Args&&... args)
{
  return oxgn_lg_detail::WithMappedArgs(
    [&](auto&... ms) -> auto {
      return fmt::format(fmt::runtime(fmt_str), ms...);
    },
    std::forward<Args>(args)...);
}

#endif // LOGURU_USE_FMTLIB
