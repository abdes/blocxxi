//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <chrono>

#include <Nova/Base/Compilers.h>
#include <gtest/gtest.h>

#include <Blocxxi/P2P/kademlia/mainline_session.h>

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

} // namespace blocxxi::p2p::kademlia
NOVA_DIAGNOSTIC_POP
