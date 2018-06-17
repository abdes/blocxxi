//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <gtest/gtest.h>

#include <common/logging.h>

namespace blocxxi {
namespace logging {

TEST(CommonLibTests, TestLoggable) {
  class Foo : Loggable<Id::TESTING> {
   public:
    Foo() { BXLOG(trace, "Foo construcor"); }
  };

  BXLOG_MISC(info, "Hello World!");
  Foo foo;
}

TEST(CommonLibTests, TestMultipleThreads) {
  std::thread th1([]() {
    for (auto ii = 0; ii < 5; ++ii)
      BXLOG_MISC(debug, "Logging from thread 1: {}", ii);
  });
  std::thread th2([]() {
    auto &test_logger = Registry::GetLogger(Id::TESTING);
    for (auto ii = 0; ii < 5; ++ii)
      BXLOG_TO_LOGGER(test_logger, trace, "Logging from thread 2: {}", ii);
  });
  th1.join();
  th2.join();
}

TEST(CommonLibTests, TestLogWithPrefix) {
  auto &test_logger = Registry::GetLogger(Id::TESTING);

  BX_DO_LOG(test_logger, debug, "message");
  BX_DO_LOG(test_logger, debug, "message {}", 1);
  BX_DO_LOG(test_logger, debug, "message {} {}", 1, 2);
  BX_DO_LOG(test_logger, debug, "message {} {} {}", 1, 2, 3);
  BX_DO_LOG(test_logger, debug, "message {} {} {} {}", 1, 3, 3, 4);
}

}  // namespace logging
}  // namespace blocxxi
