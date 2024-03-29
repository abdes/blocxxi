//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <functional> // for std::function
#include <utility>    // for std::move

#include <logging/logging.h>

namespace blocxxi {

class RunnerBase : public asap::logging::Loggable<RunnerBase> {
public:
  /// The logger id used for logging within this class.
  static constexpr const char *LOGGER_NAME = "simple-node";

  using shutdown_function_type = std::function<void()>;

  explicit RunnerBase(shutdown_function_type func)
      : shutdown_function_(std::move(func)) {
  }

  virtual ~RunnerBase() = default;

  RunnerBase(const RunnerBase &) = delete;
  auto operator=(const RunnerBase &) -> RunnerBase & = delete;

  RunnerBase(RunnerBase &&) = delete;
  auto operator=(RunnerBase &&) -> RunnerBase & = delete;

  virtual void Run() = 0;

protected:
  void CallShutdownFunction() const {
    shutdown_function_();
  }

private:
  shutdown_function_type shutdown_function_;
};

} // namespace blocxxi
