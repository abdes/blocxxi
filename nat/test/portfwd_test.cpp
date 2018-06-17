//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <gtest/gtest.h>

#include <nat/nat.h>

using blocxxi::logging::Logger;
using blocxxi::logging::Registry;
using blocxxi::nat::GetPortMapper;
using blocxxi::nat::PortMapper;

TEST(PortFwdTest, Example) {
  auto &logger = Registry::GetLogger(blocxxi::logging::Id::TESTING);

  int port = 7272;
  auto pf = GetPortMapper("");
  if (!pf) {
    BXLOG_TO_LOGGER(logger, debug, "port forwarder init failed.");
  } else {
    BXLOG_TO_LOGGER(logger, info, "External IP : {}", pf->ExternalIP());
    BXLOG_TO_LOGGER(logger, info, "Internal IP : {}", pf->InternalIP());

    auto failure = pf->AddMapping(PortMapper::Protocol::UDP, 7272, 7272, "test",
                                  std::chrono::seconds(0));
    if (failure) {
      BXLOG_TO_LOGGER(logger, debug, "port ({}) forwarding failed.", port);
    } else {
      failure = pf->DeleteMapping(PortMapper::Protocol::UDP, port);
      if (failure) {
        BXLOG_TO_LOGGER(logger, debug, "port ({}) forwarding removal failed.",
                        port);
      }
    }
  }
}
