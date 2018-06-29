//    Copyright The asap Project Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <console_runner.h>

#include <boost/asio.hpp>

namespace asap {

ConsoleRunner::ConsoleRunner(RunnerBase::shutdown_function_type f)
    : RunnerBase(std::move(f)) {
  io_context_ = new boost::asio::io_context(1);
  signals_ = new boost::asio::signal_set(*io_context_);
  // Register to handle the signals that indicate when the server should exit.
  // It is safe to register for the same signal multiple times in a program,
  // provided all registration for the specified signal is made through Asio.
  signals_->add(SIGINT);
  signals_->add(SIGTERM);
}
ConsoleRunner::~ConsoleRunner() {
  if (!io_context_->stopped()) io_context_->stop();
  delete signals_;
  delete io_context_;
}

void ConsoleRunner::Run() {
  signals_->async_wait([this](boost::system::error_code /*ec*/, int /*signo*/) {
    ASLOG(info, "Signal caught");
    // The server is stopped by cancelling all outstanding asynchronous
    // operations.
    shutdown_function_();
    // Once all operations have finished the io_context::run() call will
    // exit.
    io_context_->stop();
  });
  io_context_->run();
}

void ConsoleRunner::Run(AbstractApplication &/*app*/) {
  Run();
}

}  // namespace asap