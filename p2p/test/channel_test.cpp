//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <common/compilers.h>

#include <gtest/gtest.h>

ASAP_DIAGNOSTIC_PUSH
#if defined(ASAP_GNUC_VERSION)
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
ASAP_DIAGNOSTIC_POP

#include <p2p/kademlia/channel.h>

using namespace boost::asio;
using namespace boost::asio::ip;

using UdpSocket = udp::socket;

namespace blocxxi::p2p::kademlia {

// NOLINTNEXTLINE
TEST(ChannelTest, CreateV4) {
  boost::asio::io_context io_context;
  // NOLINTNEXTLINE
  ASSERT_NO_THROW(AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30000"));

  // NOLINTNEXTLINE
  ASSERT_THROW(
      AsyncUdpChannel::ipv4(io_context, "bad@bad", "30000"), std::system_error);
} // namespace detailTEST(ChannelTest,CreateV4)

// TODO(Abdessattar): testing IPV6 does not work inside containers
// TEST(ChannelTest, CreateV6) {
//   boost::asio::io_context io_context;
//   ASSERT_NO_THROW(AsyncUdpChannel::ipv6(io_context, "::1", "30000"));

//   ASSERT_THROW(
//       AsyncUdpChannel::ipv6(io_context, "bad@bad", "30000"),
//       std::system_error);
//   ASSERT_THROW(AsyncUdpChannel::ipv6(io_context, "127.0.0.1", "30000"),
//       std::system_error);
// }

// NOLINTNEXTLINE
TEST(ChannelTest, SendReceive) {
  boost::asio::io_context io_context;
  auto receiver = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30001");
  auto sender = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30002");
  std::string str("Hello");

  auto received = false;
  receiver->AsyncReceive(
      [&received, str](std::error_code errc, const IpEndpoint &endpoint,
          BufferReader const &buffer) {
        ASSERT_FALSE(errc);
        ASSERT_EQ("127.0.0.1", endpoint.address_.to_string());
        ASSERT_EQ(30002, endpoint.port_);
        received = true;
        auto received_str = std::string(std::cbegin(buffer), std::cend(buffer));
        ASSERT_EQ(str, received_str);
      });

  Buffer buf(str.begin(), str.end());
  auto sent = false;
  sender->AsyncSend(buf, IpEndpoint{make_address_v4("127.0.0.1"), 30001},
      [&sent](std::error_code errc) {
        ASSERT_FALSE(errc);
        sent = true;
      });
  io_context.run();
  ASSERT_TRUE(sent);
  ASSERT_TRUE(received);
}

} // namespace blocxxi::p2p::kademlia
