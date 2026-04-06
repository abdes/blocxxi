//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Nova/Testing/GTest.h>

#include <Nova/Base/Finally.h>

using nova::Finally;

namespace {

auto f(int& i) -> void { i += 1; }
int j = 0;
auto g() -> void { j += 1; }

//! Scenario: Finally executes a lambda at scope exit.
NOLINT_TEST(FinallyTest, WhenScopeExits_ThenLambdaIsExecuted)
{
  constexpr int test_value = 42;
  int i = 0;
  {
    auto f = Finally([&]() -> void { i = test_value; });
    EXPECT_EQ(i, 0);
  }
  EXPECT_EQ(i, test_value);
}

//! Scenario: Finally executes a moved lambda only once at scope exit.
NOLINT_TEST(FinallyTest, WhenMoved_ThenLambdaIsExecutedOnlyOnce)
{
  int i = 0;
  {
    auto _1 = Finally([&]() -> void { f(i); });
    {
      auto _2 = std::move(_1);
      EXPECT_TRUE(i == 0);
    }
    EXPECT_TRUE(i == 1);
    {
      // For testing purpose, we will use the object after it was moved from
      // NOLINTNEXTLINE(clang-analyzer-cplusplus.Move,bugprone-use-after-move)
      auto _2 = std::move(_1);
      EXPECT_TRUE(i == 1);
    }
    EXPECT_TRUE(i == 1);
  }
  EXPECT_TRUE(i == 1);
}

//! Scenario: Finally works with a const lvalue lambda.
NOLINT_TEST(
  FinallyTest, GivenConstLvalueLambda_WhenScopeExits_ThenLambdaIsExecuted)
{
  int i = 0;
  {
    const auto const_lvalue_lambda = [&]() -> void { f(i); };
    auto _ = Finally(const_lvalue_lambda);
    EXPECT_TRUE(i == 0);
  }
  EXPECT_TRUE(i == 1);
}

//! Scenario: Finally works with a mutable lvalue lambda.
NOLINT_TEST(
  FinallyTest, GivenMutableLvalueLambda_WhenScopeExits_ThenLambdaIsExecuted)
{
  int i = 0;
  {
    auto mutable_lvalue_lambda = [&]() -> void { f(i); };
    auto _ = Finally(mutable_lvalue_lambda);
    EXPECT_TRUE(i == 0);
  }
  EXPECT_TRUE(i == 1);
}

//! Scenario: Finally executes a lambda bound with a reference at scope exit.
NOLINT_TEST(
  FinallyTest, GivenLambdaWithReferenceBind_WhenScopeExits_ThenLambdaIsExecuted)
{
  int i = 0;
  {
    auto _ = Finally([&i]() -> void { f(i); });
    EXPECT_TRUE(i == 0);
  }
  EXPECT_TRUE(i == 1);
}

//! Scenario: Finally works with a function pointer.
NOLINT_TEST(
  FinallyTest, GivenFunctionPointer_WhenScopeExits_ThenFunctionIsExecuted)
{
  j = 0;
  {
    auto _ = Finally(&g);
    EXPECT_TRUE(j == 0);
  }
  EXPECT_TRUE(j == 1);
}

//! Scenario: Finally works with a function object.
NOLINT_TEST(
  FinallyTest, GivenFunctionObject_WhenScopeExits_ThenFunctionIsExecuted)
{
  j = 0;
  {
    auto _ = Finally(g);
    EXPECT_TRUE(j == 0);
  }
  EXPECT_TRUE(j == 1);
}

} // namespace
