//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Nova/Base/Platforms.h>
#ifdef NOVA_WINDOWS

#  include <exception>
#  include <memory>
#  include <span>
#  include <string>
#  include <system_error>
#  include <type_traits>

#  include <WTypesbase.h>
#  include <Windows.h>
#  include <comdef.h>
#  include <oaidl.h>
#  include <oleauto.h>

#  include <fmt/format.h>

#  include <Nova/Base/Logging.h>
#  include <Nova/Base/Windows/ComError.h>

using nova::string_utils::WideToUtf8;

namespace {

auto GetComErrorMessage(const HRESULT hr, IErrorInfo* help) -> std::string
{
  // NOLINTNEXTLINE(*-avoid-c-arrays)
  using ComStringPtr = std::unique_ptr<OLECHAR[], decltype(SysFreeString)*>;
  auto get_description = [](IErrorInfo* info) -> ComStringPtr {
    BSTR description = nullptr;
    if (info) {
      // Try to get the description of the error. If it fails, we cannot do
      // anything about it.
      [[maybe_unused]] auto _ = info->GetDescription(&description);
    }
    return { description, &SysFreeString };
  };

  ComStringPtr description = get_description(help);
  if (description != nullptr) {
    unsigned int length = SysStringLen(description.get());
    std::span<OLECHAR> description_span(description.get(), length);

    // Trim trailing whitespace and periods
    while (!description_span.empty()
      && (description_span.back() == L'\r' || description_span.back() == L'\n'
        || description_span.back() == L'.')) {
      description_span = description_span.first(description_span.size() - 1);
    }

    std::string utf8_description;
    try {
      WideToUtf8(std::wstring(description_span.data(), description_span.size()),
        utf8_description);
    } catch (const std::exception& e) {
      LOG_F(WARNING, "Failed to convert wide string to UTF-8: {}", e.what());
      utf8_description.append("__not_available__ (")
        .append(e.what())
        .append(")");
    }
    return utf8_description;
  }

  std::string utf8_error_message;
  try {
    WideToUtf8(_com_error(hr).ErrorMessage(), utf8_error_message);
  } catch (const std::exception& e) {
    LOG_F(WARNING, "Failed to convert wide string to UTF-8: {}", e.what());
    utf8_error_message.append("__not_available__ (")
      .append(e.what())
      .append(")");
  }
  return utf8_error_message;
}

} // namespace

auto nova::windows::ComCategory() noexcept -> const std::error_category&
{
  static ComErrorCategory com_error_category;
  return com_error_category;
}

auto nova::windows::ComErrorCategory::message(const int hr) const -> std::string
{
  std::string utf8_message;
  try {
    WideToUtf8(_com_error { hr }.ErrorMessage(), utf8_message);
  } catch (const std::exception& e) {
    LOG_F(WARNING, "Failed to convert wide string to UTF-8: {}", e.what());
    utf8_message.append("__not_available__ (").append(e.what()).append(")");
  }
  return utf8_message;
}

void nova::windows::detail::HandleComErrorImpl(
  HRESULT hr, const std::string& utf8_message)
{
  if (!utf8_message.empty()) {
    LOG_F(ERROR, "{}", utf8_message.c_str());
  }

  if (FAILED(hr)) {
    std::string error_message {};

    // Query for IErrorInfo
    IErrorInfo* p_error_info = nullptr;
    if (const auto err_info_hr = GetErrorInfo(0, &p_error_info);
      err_info_hr == S_OK) {
      // Retrieve error information
      error_message.assign(GetComErrorMessage(hr, p_error_info));
      DLOG_F(1, "COM Error: 0x{:08X} - {}", hr, error_message.c_str());
      p_error_info->Release();
    } else {
      error_message.assign(fmt::format("COM Error: 0x{:08X}", hr));
      DLOG_F(1, "{}", error_message.c_str());
    }
    ComError::Throw(static_cast<ComErrorEnum>(hr), error_message);
  }
}

#endif // NOVA_WINDOWS
