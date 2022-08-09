//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <common/compilers.h>

ASAP_DIAGNOSTIC_PUSH
#if defined(ASAP_GNUC_VERSION)
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wredundant-decls"
#endif
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
ASAP_DIAGNOSTIC_POP

#include "runner_base.h"

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
