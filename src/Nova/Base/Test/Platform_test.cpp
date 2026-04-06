//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Nova/Base/Platforms.h>

#include <string>

#include <Nova/Testing/GTest.h>

namespace {

const auto kUnsupportedPlatform = "__UNSUPPORTED__";

auto PlatformInfo() -> std::string
{
#ifdef NOVA_LINUX
  return "Linux";
#elif defined(NOVA_WINDOWS)
  return "Windows";
#elif defined(NOVA_APPLE_OSX)
  return "Apple OS X";
#else
  return cUnsupportedPlatform;
#endif
}

NOLINT_TEST(Platform, PlatformDefinitionExists)
{
  const auto platform = PlatformInfo();

  EXPECT_STRNE(kUnsupportedPlatform, platform.c_str());
}

} // namespace
