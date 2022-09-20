//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include "./runner_base.h"

#include <p2p/kademlia/boost_asio.h>

namespace blocxxi {

class ConsoleRunner : public RunnerBase {
public:
  explicit ConsoleRunner(shutdown_function_type shutdown_function);

  ~ConsoleRunner() override;
  ConsoleRunner(const ConsoleRunner &) = delete;
  auto operator=(const ConsoleRunner &) -> ConsoleRunner & = delete;

  ConsoleRunner(ConsoleRunner &&) = delete;
  auto operator=(ConsoleRunner &&) -> ConsoleRunner & = delete;

  void Run() override;

private:
  boost::asio::io_context io_context_;
  /// The signal_set is used to register for process termination notifications.
  boost::asio::signal_set signals_;
};

} // namespace blocxxi
