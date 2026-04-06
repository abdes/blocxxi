//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Nova/Base/Platforms.h>

#ifdef NOVA_WINDOWS
#  include <Nova/Base/Windows/Exceptions.h>
#  include <windows.h>
#else
#  include <cstdlib>
#  include <cwchar>
#  include <locale>
#  include <string>
#  include <vector>
#endif

namespace nova::string_utils {

// Concept to ensure the output type has clear(), resize(), and data() methods
// and that T::value_type is convertible to char
template <typename T>
concept ResizableString = requires(T t) {
  { t.clear() } -> std::same_as<void>;
  { t.resize(std::declval<size_t>()) } -> std::same_as<void>;
  // ReSharper disable once CppRedundantTypenameKeyword
  { t.data() } -> std::same_as<typename T::value_type*>;
  requires std::convertible_to<typename T::value_type, char>;
};

// Generic function to convert UTF-8 to wide string
template <typename InputType>
void Utf8ToWide(const InputType& in, std::wstring& out)
{
  if (in.empty()) {
    out.clear();
    return;
  }
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  const std::string_view sv_in(
    reinterpret_cast<const char*>(in.data()), in.size());

#ifdef NOVA_WINDOWS
  const int size_needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
    sv_in.data(), static_cast<int>(sv_in.size()), nullptr, 0);
  if (size_needed <= 0) {
    windows::WindowsException::ThrowFromLastError();
  }

  out.resize(size_needed);
  const int ret = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
    sv_in.data(), static_cast<int>(sv_in.size()), out.data(), size_needed);
  if (ret <= 0) {
    windows::WindowsException::ThrowFromLastError();
  }
#else
  // Save the current locale
  std::string currentLocale = std::setlocale(LC_ALL, nullptr);

  // Set the locale to UTF-8
  std::setlocale(LC_ALL, "en_US.UTF-8");

  size_t len = std::mbstowcs(nullptr, sv_in.data(), 0);
  if (len == static_cast<size_t>(-1)) {
    // Restore the previous locale before throwing an exception
    std::setlocale(LC_ALL, currentLocale.c_str());
    throw std::runtime_error("Invalid UTF-8 sequence");
  }

  out.resize(len);
  std::mbstowcs(&out[0], sv_in.data(), len);

  // Restore the previous locale
  std::setlocale(LC_ALL, currentLocale.c_str());
#endif
}

// Overloads for different input types
inline void Utf8ToWide(const char* in, std::wstring& out)
{
  Utf8ToWide(std::string_view(in), out);
}

inline void Utf8ToWide(const char8_t* in, std::wstring& out)
{
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  Utf8ToWide(std::string_view(reinterpret_cast<const char*>(in)), out);
}

inline void Utf8ToWide(const uint8_t* in, std::wstring& out)
{
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  Utf8ToWide(std::string_view(reinterpret_cast<const char*>(in)), out);
}

inline void WideToUtf8(std::wstring_view in, std::wstring& out)
{
  out.assign(in.begin(), in.end());
}

// Generic function to convert wide string to UTF-8
template <ResizableString OutputType>
void WideToUtf8(const std::wstring_view& in, OutputType& out)
{
  if (in.empty()) {
    out.clear();
    return;
  }
#ifdef NOVA_WINDOWS
  int size_needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
    in.data(), static_cast<int>(in.size()), nullptr, 0, nullptr, nullptr);
  if (size_needed <= 0) {
    windows::WindowsException::ThrowFromLastError();
  }

  out.resize(size_needed);
  const int ret = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, in.data(),
    static_cast<int>(in.size()),
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    reinterpret_cast<char*>(out.data()), size_needed, nullptr, nullptr);
  if (ret <= 0) {
    windows::WindowsException::ThrowFromLastError();
  }
#else
  // Save the current locale
  std::string currentLocale = std::setlocale(LC_ALL, nullptr);

  // Set the locale to UTF-8
  std::setlocale(LC_ALL, "en_US.UTF-8");

  size_t len = std::wcstombs(nullptr, in.data(), 0);
  if (len == static_cast<size_t>(-1)) {
    // Restore the previous locale before throwing an exception
    std::setlocale(LC_ALL, currentLocale.c_str());
    throw std::runtime_error("Invalid wide character sequence");
  }

  out.resize(len);
  std::wcstombs(reinterpret_cast<char*>(out.data()), in.data(), len);

  // Restore the previous locale
  std::setlocale(LC_ALL, currentLocale.c_str());
#endif
}

// Overloads for different input types
inline void WideToUtf8(const std::wstring& in, std::string& out)
{
  WideToUtf8(std::wstring_view(in), out);
}

inline void WideToUtf8(const wchar_t* in, std::string& out)
{
  WideToUtf8(std::wstring_view(in), out);
}

inline void WideToUtf8(const char16_t* in, std::string& out)
{
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  WideToUtf8(std::wstring_view(reinterpret_cast<const wchar_t*>(in)), out);
}

inline void WideToUtf8(const std::wstring& in, std::u8string& out)
{
  WideToUtf8(std::wstring_view(in), out);
}

inline void WideToUtf8(const wchar_t* in, std::u8string& out)
{
  WideToUtf8(std::wstring_view(in), out);
}

inline void WideToUtf8(const char16_t* in, std::u8string& out)
{
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  WideToUtf8(std::wstring_view(reinterpret_cast<const wchar_t*>(in)), out);
}

inline void WideToUtf8(std::string_view in, std::u8string& out)
{
  out.assign(in.begin(), in.end());
}

inline void WideToUtf8(std::u8string_view in, std::u8string& out)
{
  out.assign(in.begin(), in.end());
}

inline void WideToUtf8(std::string_view in, std::string& out)
{
  out.assign(in.begin(), in.end());
}

inline void WideToUtf8(std::u8string_view in, std::string& out)
{
  out.assign(in.begin(), in.end());
}

} // namespace nova::string_utils
