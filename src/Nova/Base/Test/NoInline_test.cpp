//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Nova/Testing/GTest.h>

#include <Nova/Base/NoInline.h>

namespace {

NOVA_NOINLINE auto NoInlineFunction(const int x) -> int { return x * 2; }

//! Tests that `NoInlineFunction` multiplies its input by two.
NOLINT_TEST(NoInlineFunctionTest, MultipliesByTwo)
{
  EXPECT_EQ(NoInlineFunction(2), 4);
  EXPECT_EQ(NoInlineFunction(3), 6);
}

} // namespace
