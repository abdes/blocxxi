//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <common/compilers.h>
#include <logging/logging.h>

#include <nat/nat.h>

// Disable compiler and linter warnings originating from the unit test framework
// and for which we cannot do anything. Additionally, every TEST or TEST_X macro
// usage must be preceded by a '// NOLINTNEXTLINE'.
ASAP_DIAGNOSTIC_PUSH
#if defined(ASAP_CLANG_VERSION)
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

using asap::logging::Registry;
using blocxxi::nat::GetPortMapper;
using blocxxi::nat::PortMapper;

// NOLINTNEXTLINE
TEST(PortFwdTest, Example) {
  auto &logger = Registry::GetLogger("testing");

  const auto mapper = GetPortMapper("");
  if (!mapper) {
    ASLOG_TO_LOGGER(logger, debug, "port forwarder init failed.");
  } else {
    ASLOG_TO_LOGGER(logger, info, "External IP : {}", mapper->ExternalIP());
    ASLOG_TO_LOGGER(logger, info, "Internal IP : {}", mapper->InternalIP());

    constexpr uint16_t c_port = 7272;
    if (mapper->AddMapping({PortMapper::Protocol::UDP, c_port, c_port, "test"},
            std::chrono::seconds(0))) {
      ASLOG_TO_LOGGER(logger, debug, "port ({}) forwarding failed.", c_port);
    } else {
      if (mapper->DeleteMapping(PortMapper::Protocol::UDP, c_port)) {
        ASLOG_TO_LOGGER(
            logger, debug, "port ({}) forwarding removal failed.", c_port);
      }
    }
  }
}
ASAP_DIAGNOSTIC_POP
