//    Copyright The asap Project Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <runner_base.h>

namespace boost {
namespace asio {
class io_context;
class signal_set;
}  // namespace asio
}  // namespace boost

namespace asap {

class ConsoleRunner : public RunnerBase {
 public:
  explicit ConsoleRunner(shutdown_function_type f);

  ~ConsoleRunner() override;
  ConsoleRunner(const ConsoleRunner &) = delete;
  ConsoleRunner &operator=(const ConsoleRunner &) = delete;

  void Run(AbstractApplication &app) override;
  void Run();

 private:
  boost::asio::io_context *io_context_;
  /// The signal_set is used to register for process termination notifications.
  boost::asio::signal_set *signals_;
};

}  // namespace asap
