//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <logging/logging.h>

#include <nat/nat.h>

// Disable compiler and linter warnings originating from the unit test framework
// and for which we cannot do anything. Additionally, every TEST or TEST_X macro
// usage must be preceded by a '// NOLINTNEXTLINE'.
ASAP_DIAGNOSTIC_PUSH
#if defined(ASAP_CLANG_VERSION)
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

#include <gtest/gtest.h>

using asap::logging::Logger;
using asap::logging::Registry;
using blocxxi::nat::GetPortMapper;
using blocxxi::nat::PortMapper;

// NOLINTNEXTLINE
TEST(PortFwdTest, Example) {
  auto &logger = Registry::GetLogger("testing");

  auto pf = GetPortMapper("");
  if (!pf) {
    ASLOG_TO_LOGGER(logger, debug, "port forwarder init failed.");
  } else {
    ASLOG_TO_LOGGER(logger, info, "External IP : {}", pf->ExternalIP());
    ASLOG_TO_LOGGER(logger, info, "Internal IP : {}", pf->InternalIP());

    unsigned short port = 7272;
    auto failure = pf->AddMapping(
        PortMapper::Protocol::UDP, port, port, "test", std::chrono::seconds(0));
    if (failure) {
      ASLOG_TO_LOGGER(logger, debug, "port ({}) forwarding failed.", port);
    } else {
      failure = pf->DeleteMapping(PortMapper::Protocol::UDP, port);
      if (failure) {
        ASLOG_TO_LOGGER(
            logger, debug, "port ({}) forwarding removal failed.", port);
      }
    }
  }
}
