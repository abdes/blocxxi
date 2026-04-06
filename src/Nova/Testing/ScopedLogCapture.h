//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <Nova/Base/Logging.h>

namespace nova::testing {

//! Scoped log capture utility for tests using Loguru.
/*! Installs a Loguru callback on construction and removes it on destruction.
    Captures the message text for assertions.

    Usage example:
      ScopedLogCapture capture{"MyTestCapture", loguru::Verbosity_9};
      EXPECT_TRUE(capture.Contains("needle"));
 */
class ScopedLogCapture {
public:
  using Predicate = std::function<bool(const loguru::Message&)>;

  //! Construct and install a callback.
  /*! @param id           Unique id for the callback registration.
      @param minVerbosity Minimum verbosity to capture (inclusive).
      @param filter       Optional predicate to filter messages captured.
   */
  explicit ScopedLogCapture(std::string id = "ScopedLogCapture",
    loguru::Verbosity minVerbosity = loguru::Verbosity_9, Predicate filter = {})
    : id_(std::move(id))
    , filter_(std::move(filter))
  {
    loguru::add_callback(
      id_.c_str(), &ScopedLogCapture::OnLog, this, minVerbosity);
  }

  ScopedLogCapture(const ScopedLogCapture&) = delete;
  auto operator=(const ScopedLogCapture&) -> ScopedLogCapture& = delete;

  ~ScopedLogCapture() { (void)loguru::remove_callback(id_.c_str()); }

  //! Return true if any captured message contains the needle substring.
  [[nodiscard]] auto Contains(std::string_view needle) const -> bool
  {
    return Count(needle) > 0;
  }

  //! Count number of captured messages that contain the needle substring.
  [[nodiscard]] auto Count(std::string_view needle) const -> int
  {
    int n = 0;
    for (const auto& msg : messages_) {
      if (msg.find(needle) != std::string::npos) {
        ++n;
      }
    }
    return n;
  }

  //! Access captured messages.
  [[nodiscard]] auto Messages() const -> const std::vector<std::string>&
  {
    return messages_;
  }

  //! Clear captured messages.
  void Clear() { messages_.clear(); }

private:
  static void OnLog(void* user_data, const loguru::Message& message)
  {
    auto* self = static_cast<ScopedLogCapture*>(user_data);
    if (!self || !message.message) {
      return;
    }
    if (self->filter_ && !self->filter_(message)) {
      return;
    }
    self->messages_.emplace_back(message.message);
  }

  std::string id_;
  Predicate filter_;
  std::vector<std::string> messages_;
};

} // namespace nova::testing
