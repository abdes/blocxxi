//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <chrono>

#include <Nova/Base/Compilers.h>
#include <gtest/gtest.h>

#include <Blocxxi/P2P/kademlia/session.h>

// Disable compiler and linter warnings originating from the unit test framework
// and for which we cannot do anything. Additionally, every TEST or TEST_X macro
// usage must be preceded by a '// NOLINTNEXTLINE'.
NOVA_DIAGNOSTIC_PUSH
#if NOVA_CLANG_VERSION
#  pragma clang diagnostic ignored "-Wused-but-marked-unused"
#  pragma clang diagnostic ignored "-Wglobal-constructors"
#  pragma clang diagnostic ignored "-Wexit-time-destructors"
#  pragma clang diagnostic ignored "-Wweak-vtables"
#endif

namespace blocxxi::p2p::kademlia {

static_assert(std::is_same_v<Session, MainlineSession>);

// NOLINTNEXTLINE
TEST(MainlineSessionTest, ForwardsActiveGetPeersRoundTrip)
{
  asio::io_context io_context;
  auto server_channel = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30123");
  auto client_channel = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30124");

  auto server = MainlineSession(MainlineDhtNode(io_context,
    Node(Node::IdType::RandomHash(), "127.0.0.1", 30123),
    std::move(server_channel)));
  auto client = MainlineSession(MainlineDhtNode(io_context,
    Node(Node::IdType::RandomHash(), "127.0.0.1", 30124),
    std::move(client_channel)));

  auto info_hash = Node::IdType::RandomHash();
  server.Start();
  client.Start();

  auto token = std::string {};
  client.AsyncGetPeers(info_hash, IpEndpoint { "127.0.0.1", 30123 },
    [&token](std::error_code const& failure, KrpcMessage const& response) {
      ASSERT_FALSE(failure);
      token = std::get<KrpcResponse>(response.payload_).values_.at("token").AsString();
    });
  io_context.run_for(std::chrono::seconds(1));
  ASSERT_FALSE(token.empty());
}

// NOLINTNEXTLINE
TEST(MainlineSessionTest, ForwardsSampleInfohashesRoundTrip)
{
  asio::io_context io_context;
  auto server_channel = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30127");
  auto client_channel = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30128");

  auto info_hash = Node::IdType::RandomHash();
  auto server = MainlineSession(MainlineDhtNode(io_context,
    Node(Node::IdType::RandomHash(), "127.0.0.1", 30127),
    std::move(server_channel)));
  auto client = MainlineSession(MainlineDhtNode(io_context,
    Node(Node::IdType::RandomHash(), "127.0.0.1", 30128),
    std::move(client_channel)));

  server.Start();
  client.Start();

  auto token = std::string {};
  client.AsyncGetPeers(info_hash, IpEndpoint { "127.0.0.1", 30127 },
    [&token](std::error_code const& failure, KrpcMessage const& response) {
      ASSERT_FALSE(failure);
      token = std::get<KrpcResponse>(response.payload_).values_.at("token").AsString();
    });
  io_context.run_for(std::chrono::seconds(1));
  ASSERT_FALSE(token.empty());

  auto announce_ok = false;
  client.AsyncAnnouncePeer(
    info_hash, token, 49004, false, IpEndpoint { "127.0.0.1", 30127 },
    [&announce_ok](std::error_code const& failure, KrpcMessage const& response) {
      ASSERT_FALSE(failure);
      ASSERT_EQ(GetType(response), KrpcMessage::Type::Response);
      announce_ok = true;
    });
  io_context.run_for(std::chrono::seconds(1));
  ASSERT_TRUE(announce_ok);

  auto got_sample = false;
  client.AsyncSampleInfohashes(info_hash, IpEndpoint { "127.0.0.1", 30127 },
    [&got_sample, info_hash](std::error_code const& failure,
      KrpcMessage const& response) {
      ASSERT_FALSE(failure);
      auto const& body = std::get<KrpcResponse>(response.payload_);
      ASSERT_TRUE(body.values_.contains("samples"));
      auto const samples = body.values_.at("samples").AsString();
      ASSERT_EQ(samples.size(), Node::IdType::Size());
      ASSERT_EQ(samples, std::string(
                           reinterpret_cast<char const*>(info_hash.Data()),
                           info_hash.Size()));
      got_sample = true;
    });
  io_context.run_for(std::chrono::seconds(1));
  ASSERT_TRUE(got_sample);
}

// NOLINTNEXTLINE
TEST(MainlineSessionTest, ActiveRequestTimesOutCleanly)
{
  asio::io_context io_context;
  auto client_channel
    = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30129");

  auto client = MainlineSession(MainlineDhtNode(io_context,
    Node(Node::IdType::RandomHash(), "127.0.0.1", 30129),
    std::move(client_channel)));
  client.Start();

  auto timed_out = false;
  client.AsyncGetPeers(Node::IdType::RandomHash(),
    IpEndpoint { "127.0.0.1", 30130 },
    [&timed_out](std::error_code const& failure, KrpcMessage const&) {
      ASSERT_EQ(failure, std::make_error_code(std::errc::timed_out));
      timed_out = true;
    });

  io_context.run_for(REQUEST_TIMEOUT + std::chrono::seconds(1));
  ASSERT_TRUE(timed_out);
}

} // namespace blocxxi::p2p::kademlia
NOVA_DIAGNOSTIC_POP
