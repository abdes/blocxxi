//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <Nova/Base/Logging.h>

auto main(int argc, char** argv) -> int
{
  // Check the program was started just for test case discovery
  bool list_tests = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--gtest_list_tests") == 0) {
      list_tests = true;
      break;
    }
  }

  if (!list_tests) {
#ifdef _MSC_VER
    // Enable memory leak detection in debug mode
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    loguru::g_preamble_date = false;
    loguru::g_preamble_file = true;
    loguru::g_preamble_verbose = false;
    loguru::g_preamble_time = false;
    loguru::g_preamble_uptime = false;
    loguru::g_preamble_thread = true;
    loguru::g_preamble_header = false;
#ifndef NDEBUG
    loguru::g_global_verbosity = loguru::Verbosity_WARNING;
#else
    loguru::g_global_verbosity = loguru::Verbosity_0;
#endif // !NDEBUG
  }

  if (!list_tests) {
    // Optional, but useful to time-stamp the start of the log.
    // Will also detect verbosity level on command line as -v.
    loguru::init(argc, const_cast<const char**>(argv));
    loguru::set_thread_name("main");
  }

  testing::InitGoogleTest(&argc, argv);
  const auto ret = RUN_ALL_TESTS();

  if (!list_tests) {
    loguru::flush();
    loguru::g_global_verbosity = loguru::Verbosity_OFF;
    loguru::shutdown();
  }

  return ret;
}
