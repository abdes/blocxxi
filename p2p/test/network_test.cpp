//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <common/compilers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <p2p/kademlia/channel.h>
#include <p2p/kademlia/message_serializer.h>
#include <p2p/kademlia/network.h>

using testing::_; // NOLINT
using testing::AtLeast;
using testing::NiceMock;
using testing::Return;

// Disable compiler and linter warnings originating from the unit test framework
// and for which we cannot do anything. Additionally, every TEST or TEST_X macro
// usage must be preceded by a '// NOLINTNEXTLINE'.
ASAP_DIAGNOSTIC_PUSH
#if defined(ASAP_CLANG_VERSION)
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

namespace blocxxi::p2p::kademlia {

class MockChannel {
public:
  using RecvCallBackType = std::function<void(
      std::error_code const &, IpEndpoint const &, BufferReader const &)>;

  using SendCallbackType = std::function<void(std::error_code const &)>;

  // NOLINTNEXTLINE
  MOCK_METHOD1(AsyncReceive, void(RecvCallBackType));
  // NOLINTNEXTLINE
  MOCK_METHOD3(AsyncSend,
      void(std::vector<uint8_t> const &, IpEndpoint const &, SendCallbackType));
  // NOLINTNEXTLINE
  MOCK_CONST_METHOD0(LocalEndpoint, IpEndpoint());
};

// NOLINTNEXTLINE
TEST(NetworkTest, Simple) {
  boost::asio::io_context context;
  auto v4_chan = AsyncUdpChannel::ipv4(context, "127.0.0.1", "4242");
  // TODO(Abdessattar): testing IPV6 does not work inside containers
  // auto v6 = AsyncUdpChannel::ipv6(context, "::1", "4242");
  auto serializer =
      std::make_unique<MessageSerializer>(Node::IdType::RandomHash());

  Network<AsyncUdpChannel, MessageSerializer> network(
      context, std::move(serializer), std::move(v4_chan));
  context.run();
}

// TODO(Abdessattar) this test relies on assertion failure
// TEST(NetworkTest, CallingStartWithNoReceiveHandlerIsFatal) {
//   boost::asio::io_context context;
//   auto v4 = std::make_unique<NiceMock<MockChannel>>();
//   auto v6 = std::make_unique<NiceMock<MockChannel>>();
//   auto serializer =
//       std::make_unique<MessageSerializer>(Node::IdType::RandomHash());
//   Network<MockChannel, MessageSerializer> network(
//       context, std::move(serializer), std::move(v4), std::move(v6));
//   /*
//    Assertion failed. Please file a bugreport at ...

//    file:   '/Users/abdessattar/Projects/blocxxi/p2p ...
//    line: 118
//    function: void blocxxi::p2p::kademlia::Network<blocxxi::p2p::...
//   */
//   auto regex =
//       "Assertion failed.*\n"
//       ".*\n"
//       "file:.*\n"
//       "line:.*\n"
//       "function:.*\n";
//   ASSERT_DEATH(network.Start(), regex);
// }

// NOLINTNEXTLINE
TEST(NetworkTest, StartSchedulesRceiveOnAllchannels) {
  boost::asio::io_context context;
  auto v4_chan = std::make_unique<NiceMock<MockChannel>>();
  auto v6_chan = std::make_unique<NiceMock<MockChannel>>();

  ON_CALL(*v4_chan, LocalEndpoint())
      .WillByDefault(Return(IpEndpoint{"127.0.0.1", 4242}));
  ON_CALL(*v6_chan, LocalEndpoint())
      .WillByDefault(Return(IpEndpoint{"::1", 4242}));

  EXPECT_CALL(*v4_chan, AsyncReceive(_)).Times(AtLeast(1));
  EXPECT_CALL(*v6_chan, AsyncReceive(_)).Times(AtLeast(1));

  auto serializer =
      std::make_unique<MessageSerializer>(Node::IdType::RandomHash());
  Network<MockChannel, MessageSerializer> network(
      context, std::move(serializer), std::move(v4_chan), std::move(v6_chan));

  network.OnMessageReceived([](IpEndpoint const &, BufferReader const &) {});
  network.Start();
}

} // namespace blocxxi::p2p::kademlia
ASAP_DIAGNOSTIC_POP
