//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Nova/Base/Platforms.h>
#ifdef NOVA_WINDOWS

#  include <Nova/Base/Windows/Exceptions.h>

#  include <memory>
#  include <string>

#  include <fmt/format.h>

#  include <Nova/Base/Finally.h>
#  include <Nova/Base/Logging.h>
#  include <Nova/Base/StringUtils.h>

using nova::string_utils::WideToUtf8;
using nova::windows::WindowsException;

namespace {

auto GetErrorMessage(const DWORD error_code) -> std::string
{
  try {
    LPWSTR buffer { nullptr };

    const DWORD buffer_length = FormatMessageW(
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, nullptr,
      error_code, 0,
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);

    if (buffer_length == 0) {
      return fmt::format(
        "__not_available__ (failed to get error message `{}`)", GetLastError());
    }

    // Ensure buffer is freed when we exit this scope
    auto cleanup = nova::Finally([buffer]() -> void {
      if (buffer) {
        LocalFree(buffer);
      }
    });

    std::string message {};
    WideToUtf8(buffer, message);
    return message;

  } catch (const std::exception& e) {
    return fmt::format("__not_available__ ({})", e.what());
  }
}

} // namespace

auto WindowsException::FromErrorCode(const DWORD error_code) noexcept
  -> std::exception_ptr
{
  return std::make_exception_ptr(WindowsException(error_code));
}

auto WindowsException::what() const noexcept -> const char*
{
  if (!message_.has_value()) {
    message_
      = fmt::format("{} : {}", code().value(), GetErrorMessage(code().value()));
  }
  return message_->c_str();
}

#endif // NOVA_WINDOWS
