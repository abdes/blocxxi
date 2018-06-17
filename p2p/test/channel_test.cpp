//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <gtest/gtest.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>

#include <p2p/kademlia/channel.h>

using namespace boost::asio;
using namespace boost::asio::ip;

using UdpSocket = udp::socket;

namespace blocxxi {
namespace p2p {
namespace kademlia {

TEST(ChannelTest, CreateV4) {
  boost::asio::io_context io_context;
  ASSERT_NO_THROW(AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "3000"));

  ASSERT_THROW(AsyncUdpChannel::ipv4(io_context, "bad@bad", "3000"),
               std::system_error);
  ASSERT_THROW(AsyncUdpChannel::ipv4(io_context, "::1", "3000"),
               std::system_error);
}  // namespace detailTEST(ChannelTest,CreateV4)

TEST(ChannelTest, CreateV6) {
  boost::asio::io_context io_context;
  ASSERT_NO_THROW(AsyncUdpChannel::ipv6(io_context, "::1", "3000"));

  ASSERT_THROW(AsyncUdpChannel::ipv6(io_context, "bad@bad", "3000"),
               std::system_error);
  ASSERT_THROW(AsyncUdpChannel::ipv6(io_context, "127.0.0.1", "3000"),
               std::system_error);
}

TEST(ChannelTest, SendReceive) {
  boost::asio::io_context io_context;
  auto receiver = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30001");
  auto sender = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30002");
  std::string str("Hello");

  auto received = false;
  receiver->AsyncReceive([&received, str](std::error_code ec, IpEndpoint ep,
                                          BufferReader const &buffer) {
    ASSERT_FALSE(ec);
    ASSERT_EQ("127.0.0.1", ep.address_.to_string());
    ASSERT_EQ(30002, ep.port_);
    received = true;
    auto received_str = std::string(buffer.cbegin(), buffer.cend());
    ASSERT_EQ(str, received_str);
  });

  Buffer buf(str.begin(), str.end());
  auto sent = false;
  sender->AsyncSend(buf, IpEndpoint{make_address_v4("127.0.0.1"), 30001},
                    [&sent](std::error_code ec) {
                      ASSERT_FALSE(ec);
                      sent = true;
                    });
  io_context.run();
  ASSERT_TRUE(sent);
  ASSERT_TRUE(received);
}

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi
