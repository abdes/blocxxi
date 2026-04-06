//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Nova/Base/Platforms.h>
#ifdef NOVA_WINDOWS

#  include <optional>
#  include <string>
#  include <system_error>

#  include <Nova/Base/StringTypes.h>
#  include <Nova/Base/StringUtils.h>
#  include <Nova/Base/api_export.h>

namespace nova {
enum class ComErrorEnum;
}

// Make com_error_enum known to std library
template <>
struct std::is_error_code_enum<nova::ComErrorEnum> : std::true_type { };

namespace nova::windows {

// Enum for classifying HRESULT error codes
enum class ComErrorEnum { };

// Access the COM error category
NOVA_BASE_API auto ComCategory() noexcept -> const std::error_category&;

// Function to create std::error_code from com_error_enum
inline auto make_error_code(ComErrorEnum e) noexcept -> std::error_code
{
  return { static_cast<int>(e), ComCategory() };
}

// Custom error category for COM errors
class ComErrorCategory final : public std::error_category {
public:
  [[nodiscard]] auto name() const noexcept -> const char* override
  {
    return "com";
  }
  [[nodiscard]] auto default_error_condition(const int hr) const noexcept
    -> std::error_condition override
  {
    return (HRESULT_CODE(hr) || hr == 0)
      ? std::system_category().default_error_condition(HRESULT_CODE(hr))
      : std::error_condition { hr, ComCategory() };
  }
  [[nodiscard]] NOVA_BASE_API auto message(int hr) const
    -> std::string override;
};

// ComError class derived from std::system_error
class ComError final : public std::system_error {
public:
  explicit ComError(const ComErrorEnum error_code)
    : std::system_error(static_cast<int>(error_code), ComCategory())
  {
  }

  ComError(ComErrorEnum error_code, const char* msg)
    : std::system_error(static_cast<int>(error_code), ComCategory(), msg)
  {
  }

  ComError(ComErrorEnum error_code, const std::string& msg)
    : std::system_error(static_cast<int>(error_code), ComCategory(), msg)
  {
  }

  static __declspec(noreturn) auto Throw(
    const ComErrorEnum error_code, const std::string& utf8_message) -> void
  {
    throw ComError(error_code, utf8_message);
  }

  // ReSharper disable once CppInconsistentNaming
  [[nodiscard]] auto GetHR() const noexcept -> HRESULT
  {
    return code().value();
  }

  //
  // private:
  //    mutable std::optional<std::string> message_;
};

namespace detail {

  // Non-templated function to handle COM errors with a UTF-8 message
  NOVA_BASE_API auto HandleComErrorImpl(
    HRESULT hr, const std::string& utf8_message) -> void;

  // Define a concept for nullable types
  template <typename T>
  concept Nullable = requires(T t) {
    { t == nullptr } -> std::convertible_to<bool>;
  };

  // Function to handle COM errors
  template <string_utils::StringType T>
  auto HandleComError(const HRESULT hr, T message = nullptr) -> void
  {
    std::string utf8_message {};
    if constexpr (Nullable<T>) {
      HandleComErrorImpl(hr, utf8_message);
    } else {
      try {
        string_utils::WideToUtf8(message, utf8_message);
      } catch (const std::exception& e) {
        utf8_message.append("__not_available__ (").append(e.what()).append(")");
      }
      HandleComErrorImpl(hr, utf8_message);
    }
  }

} // namespace detail

// Inline function to call a function and check the HRESULT
template <string_utils::StringType T>
auto ThrowOnFailed(const HRESULT hr, T message) -> void
{
  if (FAILED(hr)) {
    detail::HandleComError(hr, message);
  }
}

inline auto ThrowOnFailed(const HRESULT hr) -> void
{
  if (FAILED(hr)) {
    detail::HandleComError<const char*>(hr);
  }
}

} // namespace nova::windows

#endif // NOVA_WINDOWS
