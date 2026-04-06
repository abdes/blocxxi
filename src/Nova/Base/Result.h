//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

/*!
  @file Result.h
  @brief Defines the Result class template for representing the outcome of an
  operation.

  The Result class template is a C++ implementation of a common pattern
  used to represent the outcome of an operation that can either succeed or fail.
  This pattern is often referred to as the "Result" or "Outcome" pattern and is
  widely used in various programming languages, including Rust, Swift, and
  others.

  Key Features
  - **Type Safety**: The Result class uses std::variant to hold either a value
    of type T or a std::error_code. This ensures that the result is always in a
    valid state and can only be one of the two types.
  - **Error Handling**: By encapsulating the error state within the Result
    class, it provides a clear and explicit way to handle errors, avoiding the
    need for exceptions or error codes scattered throughout the code.
  - **Move Semantics**: The class supports move semantics, which is important
    for performance, especially when dealing with large or non-trivially
    copyable types.
  - **Specialization for void**: The specialization for void allows the pattern
    to be used for functions that do not return a value but still need to
    indicate success or failure.

  Fit with C++ Programming Model The Result pattern fits well with the C++
  programming model for several reasons:
  - **RAII and Resource Management**: C++ emphasizes Resource Acquisition Is
    Initialization (RAII) for resource management. The Result class can be used
    to manage resources by ensuring that resources are only acquired when the
    operation is successful.
  - **Type Safety and Strong Typing**: C++ encourages type safety and strong
    typing. The Result class leverages these principles by using std::variant to
    ensure that the result is always in a valid state.
  - **Performance**: C++ is known for its performance characteristics. The
    Result class supports move semantics, which helps in maintaining performance
    by avoiding unnecessary copies.

  Future Evolution Towards C++23 and Later The C++ standard is continuously
  evolving, and several features in C++23 and beyond will further enhance the
  usability and performance of the Result pattern:
  - **std::expected**: C++23 introduces std::expected, which is a standardized
    version of the Result pattern. It provides a similar mechanism to represent
    success or failure and will likely become the preferred way to handle such
    cases in the future.
  - **Improved std::variant**: Future improvements to std::variant and other
    standard library components will continue to enhance the performance and
    usability of the Result class.
  - **Coroutines**: With the introduction of coroutines in C++20, the Result
    pattern can be used in conjunction with coroutines to handle asynchronous
    operations more effectively.
  - **Pattern Matching**: Future versions of C++ may introduce pattern matching,
    which will make it easier to work with std::variant and similar types,
    further simplifying the use of the Result pattern.
*/

#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>

namespace nova {

//! Wrapper type for explicitly constructing a successful Result.
template <typename T> struct OkValue {
  T value;
};

//! Wrapper type for explicitly constructing a failed Result.
template <typename E> struct ErrValue {
  E error;
};

//! Create an OkValue wrapper.
template <typename T>
[[nodiscard]] constexpr auto Ok(T&& value) -> OkValue<std::decay_t<T>>
{
  return OkValue<std::decay_t<T>> { .value = std::forward<T>(value) };
}

//! Create an ErrValue wrapper.
template <typename E>
[[nodiscard]] constexpr auto Err(E&& error) -> ErrValue<std::decay_t<E>>
{
  return ErrValue<std::decay_t<E>> { .error = std::forward<E>(error) };
}

//! Create an ErrValue wrapper from a std::errc.
[[nodiscard]] inline auto Err(const std::errc error)
  -> ErrValue<std::error_code>
{
  return ErrValue<std::error_code> { .error = std::make_error_code(error) };
}

//! A template class to represent the result of an operation.
/*!
  The Result class encapsulates a value of type T or an error code. It
  provides methods to check if the operation was successful and to retrieve
  the value or error.
*/
template <typename T, typename E = std::error_code> class [[nodiscard]] Result {
public:
  //! Constructor that initializes the Result with a value.
  explicit constexpr Result(T value) noexcept(
    std::is_nothrow_move_constructible_v<T>)
    : value_(std::move(value))
  {
  }

  //! Constructor that initializes the Result with an error code.
  explicit constexpr Result(E error) noexcept(
    std::is_nothrow_move_constructible_v<E>)
    : value_(error)
  {
  }

  //! Constructor that initializes the Result with an OkValue wrapper.
  template <typename U>
  explicit(false) constexpr Result(OkValue<U> ok) noexcept(
    std::is_nothrow_constructible_v<T, U&&>)
    : value_(T(std::forward<U>(ok.value)))
  {
  }

  //! Constructor that initializes the Result with an ErrValue wrapper.
  template <typename F>
  explicit(false) constexpr Result(ErrValue<F> err) noexcept(
    std::is_nothrow_constructible_v<E, F&&>)
    : value_(E(std::forward<F>(err.error)))
  {
  }

  //! Factory: create a successful Result.
  template <typename U>
  [[nodiscard]] static constexpr auto Ok(U&& value) noexcept(
    std::is_nothrow_constructible_v<T, U&&>) -> Result
  {
    return ::nova::Ok(std::forward<U>(value));
  }

  //! Factory: create a failed Result.
  template <typename F>
  [[nodiscard]] static constexpr auto Err(F&& error) noexcept(
    std::is_nothrow_constructible_v<E, F&&>) -> Result
  {
    return ::nova::Err(std::forward<F>(error));
  }

  //! Checks if the Result contains a value.
  [[nodiscard]] constexpr auto has_value() const noexcept -> bool
  {
    return std::holds_alternative<T>(value_);
  }

  //! Checks if the Result contains an error.
  [[nodiscard]] constexpr auto has_error() const noexcept -> bool
  {
    return std::holds_alternative<E>(value_);
  }

  //! Retrieves a pointer to the contained value if present.
  [[nodiscard]] constexpr auto value_if() noexcept -> T*
  {
    return std::get_if<T>(&value_);
  }

  //! Retrieves a pointer to the contained value if present.
  [[nodiscard]] constexpr auto value_if() const noexcept -> const T*
  {
    return std::get_if<T>(&value_);
  }

  //! Retrieves a pointer to the contained error if present.
  [[nodiscard]] constexpr auto error_if() noexcept -> E*
  {
    return std::get_if<E>(&value_);
  }

  //! Retrieves a pointer to the contained error if present.
  [[nodiscard]] constexpr auto error_if() const noexcept -> const E*
  {
    return std::get_if<E>(&value_);
  }

  //! Retrieves the value from the Result as a constant reference.
  [[nodiscard]] constexpr auto value() const& -> const T&
  {
    return std::get<T>(value_);
  }

  //! Retrieves the value from the Result as a mutable reference.
  constexpr auto value() & -> T& { return std::get<T>(value_); }

  //! Retrieves the value from the Result as a rvalue reference.
  constexpr auto value() && -> T&& { return std::get<T>(std::move(value_)); }

  //! Moves the value from the Result.
  constexpr auto move_value() noexcept -> T
  {
    return std::get<T>(std::move(value_));
  }

  //! Retrieves the error code from the Result.
  [[nodiscard]] constexpr auto error() const& -> const E&
  {
    return std::get<E>(value_);
  }

  //! Retrieves the error code from the Result as a mutable reference.
  constexpr auto error() & -> E& { return std::get<E>(value_); }

  //! Retrieves the error code from the Result as a rvalue reference.
  constexpr auto error() && -> E&& { return std::get<E>(std::move(value_)); }

  //! Checks if the Result contains a value.
  constexpr explicit operator bool() const noexcept { return has_value(); }

  //! @{
  //! Retrieves the value or a default value if the Result contains an error.
  /*!
    @param default_value The value to return if the Result contains an error.
  */
  template <typename U>
  constexpr auto value_or(U&& default_value) const& noexcept -> T
  {
    return has_value() ? value()
                       : static_cast<T>(std::forward<U>(default_value));
  }

  template <typename U>
  constexpr auto value_or(U&& default_value) && noexcept -> T
  {
    return has_value() ? std::move(value())
                       : static_cast<T>(std::forward<U>(default_value));
  }
  //! @}

  //! @{
  //! Retrieves a reference to the contained value if the Result holds a value.
  constexpr auto operator*() const& noexcept -> const T& { return value(); }

  constexpr auto operator*() & -> T& { return std::get<T>(value_); }

  constexpr auto operator*() && noexcept -> T&& { return std::move(value()); }
  //! @}

  //! @{
  //! Pointer-like access to the contained value.
  constexpr auto operator->() const noexcept -> const T* { return &value(); }

  constexpr auto operator->() noexcept -> T* { return &value(); }
  //! @}

private:
  std::variant<T, E> value_;
};

//! Specialization of the Result class for void.
/*!
  This specialization is used for operations that do not return a value but
  still need to indicate success or failure.
*/
template <typename E> class Result<void, E> {
public:
  //! Default constructor that initializes the Result with a success state.
  constexpr Result() noexcept
    : value_(std::monostate {})
  {
  }

  //! Constructor that initializes the Result with an error code.
  explicit constexpr Result(E error) noexcept(
    std::is_nothrow_move_constructible_v<E>)
    : value_(error)
  {
  }

  //! Constructor that initializes the Result with an ErrValue wrapper.
  template <typename F>
  explicit(false) constexpr Result(ErrValue<F> err) noexcept(
    std::is_nothrow_constructible_v<E, F&&>)
    : value_(E(std::forward<F>(err.error)))
  {
  }

  //! Factory: create a successful Result.
  [[nodiscard]] static constexpr auto Ok() noexcept -> Result
  {
    return Result {};
  }

  //! Factory: create a failed Result.
  template <typename F>
  [[nodiscard]] static constexpr auto Err(F&& error) noexcept(
    std::is_nothrow_constructible_v<E, F&&>) -> Result
  {
    return ::nova::Err(std::forward<F>(error));
  }

  //! Checks if the Result indicates success.
  [[nodiscard]] constexpr auto has_value() const noexcept -> bool
  {
    return std::holds_alternative<std::monostate>(value_);
  }

  //! Checks if the Result contains an error.
  [[nodiscard]] constexpr auto has_error() const noexcept -> bool
  {
    return std::holds_alternative<E>(value_);
  }

  //! Retrieves a pointer to the contained error if present.
  [[nodiscard]] constexpr auto error_if() noexcept -> E*
  {
    return std::get_if<E>(&value_);
  }

  //! Retrieves a pointer to the contained error if present.
  [[nodiscard]] constexpr auto error_if() const noexcept -> const E*
  {
    return std::get_if<E>(&value_);
  }

  //! Retrieves the error code from the Result.
  [[nodiscard]] constexpr auto error() const& -> const E&
  {
    return std::get<E>(value_);
  }

  //! Retrieves the error code from the Result as a mutable reference.
  constexpr auto error() & -> E& { return std::get<E>(value_); }

  //! Checks if the Result indicates success.
  constexpr explicit operator bool() const noexcept { return has_value(); }

private:
  std::variant<std::monostate, E> value_;
};

//! Helper macro to evaluate and expression that returns a result, and if the
//! result has an error, propagates the error.
// NOLINTNEXTLINE(*-macro-usage)
#define CHECK_RESULT(expr)                                                     \
  do {                                                                         \
    if (const auto result = (expr); !result) {                                 \
      return ::nova::Err(result.error());                                      \
    }                                                                          \
  } while (false)

} // namespace nova
