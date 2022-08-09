//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include "console_runner.h"

namespace asap {

ConsoleRunner::ConsoleRunner(
    RunnerBase::shutdown_function_type shutdown_function)
    : RunnerBase(std::move(shutdown_function)), io_context_(1),
      signals_(io_context_) {
  // Register to handle the signals that indicate when the server should exit.
  // It is safe to register for the same signal multiple times in a program,
  // provided all registration for the specified signal is made through Asio.
  signals_.add(SIGINT);
  signals_.add(SIGTERM);
}
ConsoleRunner::~ConsoleRunner() {
  if (!io_context_.stopped()) {
    io_context_.stop();
  }
}

void ConsoleRunner::Run() {
  signals_.async_wait([this](boost::system::error_code /*ec*/, int /*signo*/) {
    ASLOG(info, "Signal caught");
    // The server is stopped by cancelling all outstanding asynchronous
    // operations.
    CallShutdownFunction();
    // Once all operations have finished the io_context::run() call will
    // exit.
    io_context_.stop();
  });
  io_context_.run();
}

} // namespace asap
