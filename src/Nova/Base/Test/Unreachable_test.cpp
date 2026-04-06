//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Nova/Testing/GTest.h>

#include <Nova/Base/Unreachable.h>

#include <Nova/Base/Compilers.h>

namespace {

NOVA_DIAGNOSTIC_PUSH
NOVA_DIAGNOSTIC_DISABLE_MSVC(4702)
NOVA_DIAGNOSTIC_DISABLE_CLANG("-Wunreachable-code")
NOVA_DIAGNOSTIC_DISABLE_GCC("-Wunreachable-code")

//! Tests that unreachable code paths are not taken.
NOLINT_TEST(UnreachableTest, IsNeverReached)
{
  // ReSharper disable All
  bool unreachable = false;
  if (unreachable) {
    nova::Unreachable();
    ADD_FAILURE() << "Unreachable code was reached";
  }
}
NOVA_DIAGNOSTIC_POP

} // namespace
