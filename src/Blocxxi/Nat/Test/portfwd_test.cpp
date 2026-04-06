//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <Nova/Base/Compilers.h>
#include <Nova/Base/Logging.h>

#include <Blocxxi/Nat/nat.h>

// Disable compiler and linter warnings originating from the unit test framework
// and for which we cannot do anything. Additionally, every TEST or TEST_X macro
// usage must be preceded by a '// NOLINTNEXTLINE'.
NOVA_DIAGNOSTIC_PUSH
#if NOVA_CLANG_VERSION
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

using blocxxi::nat::GetPortMapper;
using blocxxi::nat::PortMapper;

// NOLINTNEXTLINE
TEST(PortFwdTest, Example) {
  const auto mapper = GetPortMapper("");
  if (!mapper) {
    LOG_F(1, "port forwarder init failed.");
  } else {
    LOG_F(INFO, "External IP : {}", mapper->ExternalIP());
    LOG_F(INFO, "Internal IP : {}", mapper->InternalIP());

    constexpr uint16_t c_port = 7272;
    if (mapper->AddMapping({PortMapper::Protocol::UDP, c_port, c_port, "test"},
            std::chrono::seconds(0))) {
      LOG_F(1, "port ({}) forwarding failed.", c_port);
    } else {
      if (mapper->DeleteMapping(PortMapper::Protocol::UDP, c_port)) {
        LOG_F(1, "port ({}) forwarding removal failed.", c_port);
      }
    }
  }
}
NOVA_DIAGNOSTIC_POP
