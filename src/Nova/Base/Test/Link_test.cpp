//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Nova/Base/Logging.h>

auto main(int argc, char** argv) -> int
{
  loguru::g_preamble_date = false;
  loguru::g_preamble_time = false;
  loguru::g_preamble_uptime = false;
  loguru::g_preamble_thread = false;
  loguru::g_preamble_header = false;
  loguru::g_global_verbosity = loguru::Verbosity_INFO;
  loguru::init(argc, const_cast<const char**>(argv));

  LOG_F(INFO, "Hello {}", "World!");

  loguru::shutdown();
}
