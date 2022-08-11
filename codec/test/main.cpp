//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <contract/ut/gtest.h>

auto main(int argc, char *argv[]) -> int {
  // We're doing unit tests with contract checks which require initializing the
  // asap::contract library before running the tests.
  asap::contract::PrepareForTesting();

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
