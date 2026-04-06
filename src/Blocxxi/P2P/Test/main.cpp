//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

// #include <Nova/Testing/GTest.h>
// #include <Nova/Base/Logging.h>

#include <gmock/gmock.h>

auto main(int argc, char *argv[]) -> int {
  // blocxxi::logging::Registry::instance().SetLogLevel(
  //     blocxxi::logging::Logger::Level::off);

  // blocxxi::contract::PrepareForTesting();

  testing::InitGoogleTest(&argc, argv);
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
