//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <gtest/gtest.h>

#include <p2p/kademlia/timer.h>

namespace blocxxi {
namespace p2p {
namespace kademlia {

TEST(TimerTest, OneTimer) {
  boost::asio::io_context io_context;
  auto timer = Timer(io_context);

  auto fired = false;
  timer.ExpiresFromNow(std::chrono::seconds(1), [&fired]() { fired = true; });
  io_context.run();
  ASSERT_TRUE(fired);
}

TEST(TimerTest, MultipleTimersFireInOrder) {
  boost::asio::io_context io_context;
  auto timer = Timer(io_context);

  auto fired = 0;
  timer.ExpiresFromNow(std::chrono::milliseconds(1000), [&fired]() {
    ASSERT_EQ(0, fired);
    ++fired;
  });
  timer.ExpiresFromNow(std::chrono::milliseconds(1200), [&fired]() {
    ASSERT_EQ(1, fired);
    ++fired;
  });
  timer.ExpiresFromNow(std::chrono::milliseconds(1300), [&fired]() {
    ASSERT_EQ(2, fired);
    ++fired;
  });
  io_context.run();
  ASSERT_EQ(3, fired);
}

TEST(TimerTest, ShorterTimeoutFiresBeforeOthers) {
  boost::asio::io_context io_context;
  auto timer = Timer(io_context);

  auto shorter = false;
  auto later = false;
  // First schedule a long timeout
  timer.ExpiresFromNow(std::chrono::seconds(2), [&shorter, &later]() {
    ASSERT_TRUE(shorter);
    later = true;
  });

  // Schedule a shorter timeout - it must fire before the later one
  timer.ExpiresFromNow(std::chrono::seconds(1), [&shorter, &later]() {
    ASSERT_FALSE(later);
    shorter = true;
  });
  io_context.run();
  ASSERT_TRUE(shorter);
  ASSERT_TRUE(later);
}

TEST(TimerTest, MultipleTimeoutsWithSameDuration) {
  boost::asio::io_context io_context;
  auto timer = Timer(io_context);

  auto one = false;
  auto two = false;
  auto three = false;
  auto four = false;
  timer.ExpiresFromNow(std::chrono::milliseconds(1000),
                       [&one]() { one = true; });
  timer.ExpiresFromNow(std::chrono::microseconds(1000),
                       [&two]() { two = true; });
  timer.ExpiresFromNow(std::chrono::milliseconds(1100),
                       [&three]() { three = true; });
  timer.ExpiresFromNow(std::chrono::milliseconds(1050),
                       [&four]() { four = true; });
  io_context.run();
  ASSERT_TRUE(one);
  ASSERT_TRUE(two);
  ASSERT_TRUE(three);
  ASSERT_TRUE(four);
}

TEST(TimerTest, MoveConstructorMovesTimers) {
  boost::asio::io_context io_context;
  auto moved = Timer(io_context);
  auto fired = 0;
  moved.ExpiresFromNow(std::chrono::seconds(1), [&fired]() { ++fired; });

  // Move the timer (copy)
  auto copied_timer(std::move(moved));
  io_context.run();
  ASSERT_EQ(1, fired);
}

TEST(TimerTest, MoveAssignmentMovesTimers) {
  boost::asio::io_context io_context;
  auto moved = Timer(io_context);
  auto fired = 0;
  moved.ExpiresFromNow(std::chrono::seconds(1), [&fired]() { ++fired; });

  // Move the timer (copy)
  auto timer = Timer(io_context);
  timer = std::move(moved);
  io_context.run();
  ASSERT_EQ(1, fired);
}

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi
