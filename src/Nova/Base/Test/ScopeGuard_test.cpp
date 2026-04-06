//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <stdexcept>

#include <Nova/Testing/GTest.h>

#include <Nova/Base/ScopeGuard.h>

using nova::ScopeGuard;

namespace {

auto Increment(int& i) -> void { i += 1; }

struct NoExceptFuncObj {
  explicit NoExceptFuncObj(int& i)
    : i_(&i)
  {
  }
  void operator()() const noexcept { *i_ += 1; }

private:
  int* i_;
};

//! Scenario: ScopeGuard executes a lambda at scope exit.
NOLINT_TEST(ScopeGuardTest, ExecutesFunctionOnScopeExit)
{
  bool called = false;
  {
    auto guard = ScopeGuard([&called]() noexcept -> void { called = true; });
    EXPECT_FALSE(called);
  }
  EXPECT_TRUE(called);
}

//! Scenario: ScopeGuard executes function even when an exception is thrown.
NOLINT_TEST(ScopeGuardTest, ExecutesFunctionWithExceptionSafety)
{
  bool called = false;
  try {
    auto guard = ScopeGuard([&called]() noexcept -> void { called = true; });
    throw std::runtime_error("Test exception");
  } catch (const std::runtime_error& ex) {
    // Exception caught
    (void)ex;
  }
  EXPECT_TRUE(called);
}

//! Scenario: Multiple ScopeGuards in the same scope work correctly.
NOLINT_TEST(ScopeGuardTest, ExecutesFunctionWithMultipleGuards)
{
  bool called1 = false;
  bool called2 = false;
  {
    auto guard1 = ScopeGuard([&called1]() noexcept -> void { called1 = true; });
    auto guard2 = ScopeGuard([&called2]() noexcept -> void { called2 = true; });
    EXPECT_FALSE(called1);
    EXPECT_FALSE(called2);
  }
  EXPECT_TRUE(called1);
  EXPECT_TRUE(called2);
}

//! Scenario: ScopeGuard works with a lambda capturing by reference.
NOLINT_TEST(ScopeGuardTest,
  GivenLambdaWithReferenceCaptureWhenScopeExitsThenLambdaIsExecuted)
{
  int i = 0;
  {
    ScopeGuard guard([&i]() noexcept -> void { Increment(i); });
    EXPECT_EQ(i, 0);
  }
  EXPECT_EQ(i, 1);
}

//! Scenario: ScopeGuard works with a function pointer.
NOLINT_TEST(
  ScopeGuardTest, GivenFunctionPointerWhenScopeExitsThenFunctionIsExecuted)
{
  static int j = 0;
  struct Wrapper {
    static void g() noexcept { j += 1; }
  };
  j = 0;
  {
    ScopeGuard guard(&Wrapper::g);
    EXPECT_EQ(j, 0);
  }
  EXPECT_EQ(j, 1);
}

//! Scenario: ScopeGuard works with a function object.
NOLINT_TEST(
  ScopeGuardTest, GivenFunctionObjectWhenScopeExitsThenFunctionIsExecuted)
{
  int i = 0;
  {
    ScopeGuard guard(NoExceptFuncObj { i });
    EXPECT_EQ(i, 0);
  }
  EXPECT_EQ(i, 1);
}

} // namespace
