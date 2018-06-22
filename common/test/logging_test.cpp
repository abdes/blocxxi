//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <common/logging.h>

using ::testing::_;

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

class MockSink : public spdlog::sinks::sink {
public:
	MOCK_METHOD1(log, void(const spdlog::details::log_msg& msg));
	MOCK_METHOD0(flush, void());
};

TEST(CommonLibTests, TestLogPushSink) {
	auto first_sink = new MockSink();
	auto second_sink = new MockSink();

	auto first_sink_ptr = std::shared_ptr<MockSink>(first_sink);
	auto second_sink_ptr = std::shared_ptr<MockSink>(second_sink);


	EXPECT_CALL(*first_sink, log(_)).Times(1);
	EXPECT_CALL(*second_sink, log(_)).Times(0);

	auto &test_logger = Registry::GetLogger(Id::TESTING);
	Registry::PushSink(first_sink_ptr);
	BXLOG_TO_LOGGER(test_logger, debug, "message");

	EXPECT_CALL(*first_sink, log(_)).Times(0);
	EXPECT_CALL(*second_sink, log(_)).Times(1);

	Registry::PushSink(second_sink_ptr);
	BXLOG_TO_LOGGER(test_logger, debug, "message");

	Registry::PopSink();

	EXPECT_CALL(*first_sink, log(_)).Times(1);
	EXPECT_CALL(*second_sink, log(_)).Times(0);

	BXLOG_TO_LOGGER(test_logger, debug, "message");

	Registry::PopSink();

	EXPECT_CALL(*first_sink, log(_)).Times(0);
	EXPECT_CALL(*second_sink, log(_)).Times(0);

	BXLOG_TO_LOGGER(test_logger, debug, "message");
}

}  // namespace logging
}  // namespace blocxxi
