//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <chrono>
#include <string>

#include <Nova/Base/Compilers.h>
#include <gtest/gtest.h>

#include <Blocxxi/P2P/kademlia/mainline_node.h>

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

namespace {

auto MakeTestNode(std::string_view address, std::uint16_t port) -> Node
{
  return Node(Node::IdType::RandomHash(), std::string(address), port);
}

auto MakePingQuery() -> Buffer
{
  auto query = KrpcMessage {};
  query.transaction_id_ = "aa";
  query.payload_ = KrpcQuery {
    "ping",
    {
      { "id",
        blocxxi::codec::bencode::Value("abcdefghij0123456789") },
    },
  };

  auto encoded = Encode(query);
  return Buffer(encoded.begin(), encoded.end());
}

auto MakeGetPeersQuery(std::string transaction_id, Node::IdType const& target)
  -> Buffer
{
  auto query = KrpcMessage {};
  query.transaction_id_ = std::move(transaction_id);
  query.payload_ = KrpcQuery {
    "get_peers",
    {
      { "id",
        blocxxi::codec::bencode::Value("abcdefghij0123456789") },
      { "info_hash",
        blocxxi::codec::bencode::Value(std::string(
          reinterpret_cast<char const*>(target.Data()), target.Size())) },
    },
  };
  auto encoded = Encode(query);
  return Buffer(encoded.begin(), encoded.end());
}

auto MakeAnnouncePeerQuery(std::string transaction_id, Node::IdType const& target,
  std::string token, std::uint16_t port, bool implied_port) -> Buffer
{
  auto arguments = blocxxi::codec::bencode::Value::DictionaryType {
    { "id", blocxxi::codec::bencode::Value("abcdefghij0123456789") },
    { "info_hash",
      blocxxi::codec::bencode::Value(std::string(
        reinterpret_cast<char const*>(target.Data()), target.Size())) },
    { "token", blocxxi::codec::bencode::Value(std::move(token)) },
    { "port", blocxxi::codec::bencode::Value(static_cast<std::int64_t>(port)) },
  };
  if (implied_port) {
    arguments.emplace("implied_port", blocxxi::codec::bencode::Value(1));
  }

  auto query = KrpcMessage {};
  query.transaction_id_ = std::move(transaction_id);
  query.payload_ = KrpcQuery { "announce_peer", std::move(arguments) };
  auto encoded = Encode(query);
  return Buffer(encoded.begin(), encoded.end());
}

auto MakeSampleInfohashesQuery(
  std::string transaction_id, Node::IdType const& target) -> Buffer
{
  auto query = KrpcMessage {};
  query.transaction_id_ = std::move(transaction_id);
  query.payload_ = KrpcQuery {
    "sample_infohashes",
    {
      { "id", blocxxi::codec::bencode::Value("abcdefghij0123456789") },
      { "target",
        blocxxi::codec::bencode::Value(std::string(
          reinterpret_cast<char const*>(target.Data()), target.Size())) },
    },
  };
  auto encoded = Encode(query);
  return Buffer(encoded.begin(), encoded.end());
}

auto MakeCustomQuery(std::string transaction_id, std::string method) -> Buffer
{
  auto query = KrpcMessage {};
  query.transaction_id_ = std::move(transaction_id);
  query.payload_ = KrpcQuery {
    std::move(method),
    {
      { "id", blocxxi::codec::bencode::Value("abcdefghij0123456789") },
      { "key", blocxxi::codec::bencode::Value("custom-key") },
    },
  };
  auto encoded = Encode(query);
  return Buffer(encoded.begin(), encoded.end());
}

} // namespace

// NOLINTNEXTLINE
TEST(MainlineDhtNodeTest, RespondsToPingQueries)
{
  asio::io_context io_context;
  auto receiver
    = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30111");
  auto sender = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30112");

  auto node = MainlineDhtNode(
    io_context, MakeTestNode("127.0.0.1", 30111), std::move(receiver));

  auto query_seen = false;
  node.OnQuery([&query_seen](std::string_view method, IpEndpoint const&) {
    ASSERT_EQ(method, "ping");
    query_seen = true;
  });
  node.Start();

  auto response_seen = false;
  sender->AsyncReceive([&response_seen](std::error_code const& failure,
                        IpEndpoint const&, BufferReader const& buffer) {
    ASSERT_FALSE(failure);
    auto message = DecodeKrpc(std::string_view(
      reinterpret_cast<char const*>(buffer.data()), buffer.size()));
    ASSERT_EQ(GetType(message), KrpcMessage::Type::Response);
    ASSERT_EQ(message.transaction_id_, "aa");
    response_seen = true;
  });

  auto ping = MakePingQuery();
  sender->AsyncSend(ping, IpEndpoint { "127.0.0.1", 30111 },
    [](std::error_code const& failure) { ASSERT_FALSE(failure); });

  io_context.run_for(std::chrono::seconds(1));

  ASSERT_TRUE(query_seen);
  ASSERT_TRUE(response_seen);
}

// NOLINTNEXTLINE
TEST(MainlineDhtNodeTest, ReportsBootstrapSuccessOnFindNodeReply)
{
  asio::io_context io_context;
  auto bootstrap_channel
    = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30113");
  auto main_channel = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30114");

  auto bootstrap = MainlineDhtNode(
    io_context, MakeTestNode("127.0.0.1", 30113), std::move(bootstrap_channel));
  bootstrap.Start();

  auto main = MainlineDhtNode(
    io_context, MakeTestNode("127.0.0.1", 30114), std::move(main_channel));

  auto bootstrap_ok = false;
  main.OnBootstrapSuccess([&bootstrap_ok](std::string const& bootstrap_endpoint) {
    ASSERT_EQ(bootstrap_endpoint, "127.0.0.1:30113");
    bootstrap_ok = true;
  });
  main.AddBootstrapNode("127.0.0.1:30113");
  main.Start();

  io_context.run_for(std::chrono::seconds(1));

  ASSERT_TRUE(bootstrap_ok);
}

// NOLINTNEXTLINE
TEST(MainlineDhtNodeTest, GetPeersThenAnnouncePeerReturnsStoredPeer)
{
  asio::io_context io_context;
  auto receiver
    = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30115");
  auto sender = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30116");

  auto self = MakeTestNode("127.0.0.1", 30115);
  auto target = self.Id();
  auto node = MainlineDhtNode(io_context, self, std::move(receiver));
  node.Start();

  auto token = std::string {};
  auto get_peers = MakeGetPeersQuery("ga", target);
  sender->AsyncReceive([&token](std::error_code const& failure, IpEndpoint const&,
                        BufferReader const& buffer) {
    ASSERT_FALSE(failure);
    auto message = DecodeKrpc(std::string_view(
      reinterpret_cast<char const*>(buffer.data()), buffer.size()));
    auto const& response = std::get<KrpcResponse>(message.payload_);
    token = response.values_.at("token").AsString();
    ASSERT_TRUE(response.values_.contains("nodes"));
  });
  sender->AsyncSend(get_peers, IpEndpoint { "127.0.0.1", 30115 },
    [](std::error_code const& failure) { ASSERT_FALSE(failure); });
  io_context.run_for(std::chrono::seconds(1));

  ASSERT_FALSE(token.empty());

  auto announce = MakeAnnouncePeerQuery("gb", target, token, 49000, false);
  auto announce_ok = false;
  sender->AsyncReceive([&announce_ok](std::error_code const& failure,
                        IpEndpoint const&, BufferReader const& buffer) {
    ASSERT_FALSE(failure);
    auto message = DecodeKrpc(std::string_view(
      reinterpret_cast<char const*>(buffer.data()), buffer.size()));
    ASSERT_EQ(GetType(message), KrpcMessage::Type::Response);
    announce_ok = true;
  });
  sender->AsyncSend(announce, IpEndpoint { "127.0.0.1", 30115 },
    [](std::error_code const& failure) { ASSERT_FALSE(failure); });
  io_context.run_for(std::chrono::seconds(1));
  ASSERT_TRUE(announce_ok);

  auto confirm = MakeGetPeersQuery("gc", target);
  auto values_seen = false;
  sender->AsyncReceive([&values_seen](std::error_code const& failure,
                        IpEndpoint const&, BufferReader const& buffer) {
    ASSERT_FALSE(failure);
    auto message = DecodeKrpc(std::string_view(
      reinterpret_cast<char const*>(buffer.data()), buffer.size()));
    auto const& response = std::get<KrpcResponse>(message.payload_);
    ASSERT_TRUE(response.values_.contains("values"));
    auto const& values = response.values_.at("values").AsList();
    ASSERT_EQ(values.size(), 1U);
    auto peer = DecodeCompactPeer(values.front().AsString());
    ASSERT_EQ(peer.Address().to_string(), "127.0.0.1");
    ASSERT_EQ(peer.Port(), 49000);
    values_seen = true;
  });
  sender->AsyncSend(confirm, IpEndpoint { "127.0.0.1", 30115 },
    [](std::error_code const& failure) { ASSERT_FALSE(failure); });
  io_context.run_for(std::chrono::seconds(1));
  ASSERT_TRUE(values_seen);
}

// NOLINTNEXTLINE
TEST(MainlineDhtNodeTest, RejectsAnnouncePeerWithInvalidToken)
{
  asio::io_context io_context;
  auto receiver
    = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30117");
  auto sender = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30118");

  auto self = MakeTestNode("127.0.0.1", 30117);
  auto target = self.Id();
  auto node = MainlineDhtNode(io_context, self, std::move(receiver));
  node.Start();

  auto announce
    = MakeAnnouncePeerQuery("gd", target, "bad-token", 49001, false);
  auto got_error = false;
  sender->AsyncReceive([&got_error](std::error_code const& failure,
                        IpEndpoint const&, BufferReader const& buffer) {
    ASSERT_FALSE(failure);
    auto message = DecodeKrpc(std::string_view(
      reinterpret_cast<char const*>(buffer.data()), buffer.size()));
    ASSERT_EQ(GetType(message), KrpcMessage::Type::Error);
    auto const& error = std::get<KrpcError>(message.payload_);
    ASSERT_EQ(error.code_, 203);
    got_error = true;
  });
  sender->AsyncSend(announce, IpEndpoint { "127.0.0.1", 30117 },
    [](std::error_code const& failure) { ASSERT_FALSE(failure); });
  io_context.run_for(std::chrono::seconds(1));
  ASSERT_TRUE(got_error);
}

// NOLINTNEXTLINE
TEST(MainlineDhtNodeTest, SampleInfohashesReturnsStoredInfohashes)
{
  asio::io_context io_context;
  auto receiver
    = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30119");
  auto sender = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30120");

  auto self = MakeTestNode("127.0.0.1", 30119);
  auto target = self.Id();
  auto node = MainlineDhtNode(io_context, self, std::move(receiver));
  node.Start();

  auto token = std::string {};
  auto get_peers = MakeGetPeersQuery("sa", target);
  sender->AsyncReceive([&token](std::error_code const& failure, IpEndpoint const&,
                        BufferReader const& buffer) {
    ASSERT_FALSE(failure);
    auto message = DecodeKrpc(std::string_view(
      reinterpret_cast<char const*>(buffer.data()), buffer.size()));
    auto const& response = std::get<KrpcResponse>(message.payload_);
    token = response.values_.at("token").AsString();
  });
  sender->AsyncSend(get_peers, IpEndpoint { "127.0.0.1", 30119 },
    [](std::error_code const& failure) { ASSERT_FALSE(failure); });
  io_context.run_for(std::chrono::seconds(1));

  auto announce = MakeAnnouncePeerQuery("sb", target, token, 49002, false);
  sender->AsyncReceive([](std::error_code const& failure, IpEndpoint const&,
                        BufferReader const&) { ASSERT_FALSE(failure); });
  sender->AsyncSend(announce, IpEndpoint { "127.0.0.1", 30119 },
    [](std::error_code const& failure) { ASSERT_FALSE(failure); });
  io_context.run_for(std::chrono::seconds(1));

  auto sample_infohashes = MakeSampleInfohashesQuery("sc", target);
  auto got_sample = false;
  sender->AsyncReceive([&got_sample, target](std::error_code const& failure,
                        IpEndpoint const&, BufferReader const& buffer) {
    ASSERT_FALSE(failure);
    auto message = DecodeKrpc(std::string_view(
      reinterpret_cast<char const*>(buffer.data()), buffer.size()));
    auto const& response = std::get<KrpcResponse>(message.payload_);
    ASSERT_TRUE(response.values_.contains("samples"));
    ASSERT_TRUE(response.values_.contains("num"));
    auto const samples = response.values_.at("samples").AsString();
    ASSERT_EQ(samples.size(), Node::IdType::Size());
    ASSERT_EQ(samples, std::string(
                         reinterpret_cast<char const*>(target.Data()),
                         target.Size()));
    ASSERT_EQ(response.values_.at("num").AsInteger(), 1);
    got_sample = true;
  });
  sender->AsyncSend(sample_infohashes, IpEndpoint { "127.0.0.1", 30119 },
    [](std::error_code const& failure) { ASSERT_FALSE(failure); });
  io_context.run_for(std::chrono::seconds(1));
  ASSERT_TRUE(got_sample);
}

// NOLINTNEXTLINE
TEST(MainlineDhtNodeTest, ActiveClientGetPeersAndAnnouncePeerRoundTrips)
{
  asio::io_context io_context;
  auto server_channel
    = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30121");
  auto client_channel
    = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30122");

  auto info_hash = Node::IdType::RandomHash();
  auto server = MainlineDhtNode(
    io_context, MakeTestNode("127.0.0.1", 30121), std::move(server_channel));
  auto client = MainlineDhtNode(
    io_context, MakeTestNode("127.0.0.1", 30122), std::move(client_channel));
  server.Start();
  client.Start();

  auto token = std::string {};
  client.AsyncGetPeers(info_hash, IpEndpoint { "127.0.0.1", 30121 },
    [&token](std::error_code const& failure, KrpcMessage const& response) {
      ASSERT_FALSE(failure);
      auto const& body = std::get<KrpcResponse>(response.payload_);
      token = body.values_.at("token").AsString();
    });
  io_context.run_for(std::chrono::seconds(1));
  ASSERT_FALSE(token.empty());

  auto announce_ok = false;
  client.AsyncAnnouncePeer(
    info_hash, token, 49003, false, IpEndpoint { "127.0.0.1", 30121 },
    [&announce_ok](std::error_code const& failure, KrpcMessage const& response) {
      ASSERT_FALSE(failure);
      ASSERT_EQ(GetType(response), KrpcMessage::Type::Response);
      announce_ok = true;
    });
  io_context.run_for(std::chrono::seconds(1));
  ASSERT_TRUE(announce_ok);

  auto values_seen = false;
  client.AsyncGetPeers(info_hash, IpEndpoint { "127.0.0.1", 30121 },
    [&values_seen](std::error_code const& failure, KrpcMessage const& response) {
      ASSERT_FALSE(failure);
      auto const& body = std::get<KrpcResponse>(response.payload_);
      ASSERT_TRUE(body.values_.contains("values"));
      auto peer = DecodeCompactPeer(body.values_.at("values").AsList().front().AsString());
      ASSERT_EQ(peer.Port(), 49003);
      values_seen = true;
    });
  io_context.run_for(std::chrono::seconds(1));
  ASSERT_TRUE(values_seen);
}

// NOLINTNEXTLINE
TEST(MainlineDhtNodeTest, AnnouncePeerWithImpliedPortUsesSenderPort)
{
  asio::io_context io_context;
  auto receiver
    = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30125");
  auto sender = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30126");

  auto self = MakeTestNode("127.0.0.1", 30125);
  auto target = self.Id();
  auto node = MainlineDhtNode(io_context, self, std::move(receiver));
  node.Start();

  auto token = std::string {};
  sender->AsyncReceive([&token](std::error_code const& failure, IpEndpoint const&,
                        BufferReader const& buffer) {
    ASSERT_FALSE(failure);
    auto message = DecodeKrpc(std::string_view(
      reinterpret_cast<char const*>(buffer.data()), buffer.size()));
    token = std::get<KrpcResponse>(message.payload_).values_.at("token").AsString();
  });
  sender->AsyncSend(MakeGetPeersQuery("ia", target),
    IpEndpoint { "127.0.0.1", 30125 },
    [](std::error_code const& failure) { ASSERT_FALSE(failure); });
  io_context.run_for(std::chrono::seconds(1));
  ASSERT_FALSE(token.empty());

  sender->AsyncReceive([](std::error_code const& failure, IpEndpoint const&,
                        BufferReader const&) { ASSERT_FALSE(failure); });
  sender->AsyncSend(
    MakeAnnouncePeerQuery("ib", target, token, 49005, true),
    IpEndpoint { "127.0.0.1", 30125 },
    [](std::error_code const& failure) { ASSERT_FALSE(failure); });
  io_context.run_for(std::chrono::seconds(1));

  auto values_seen = false;
  sender->AsyncReceive([&values_seen](std::error_code const& failure,
                        IpEndpoint const&, BufferReader const& buffer) {
    ASSERT_FALSE(failure);
    auto message = DecodeKrpc(std::string_view(
      reinterpret_cast<char const*>(buffer.data()), buffer.size()));
    auto const& response = std::get<KrpcResponse>(message.payload_);
    auto peer = DecodeCompactPeer(response.values_.at("values").AsList().front().AsString());
    ASSERT_EQ(peer.Port(), 30126);
    values_seen = true;
  });
  sender->AsyncSend(MakeGetPeersQuery("ic", target),
    IpEndpoint { "127.0.0.1", 30125 },
    [](std::error_code const& failure) { ASSERT_FALSE(failure); });
  io_context.run_for(std::chrono::seconds(1));
  ASSERT_TRUE(values_seen);
}

// NOLINTNEXTLINE
TEST(MainlineDhtNodeTest, CustomQueryResponderMergesCustomPayloadIntoResponse)
{
  asio::io_context io_context;
  auto receiver
    = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30131");
  auto sender = AsyncUdpChannel::ipv4(io_context, "127.0.0.1", "30132");

  auto node = MainlineDhtNode(
    io_context, MakeTestNode("127.0.0.1", 30131), std::move(receiver));
  node.OnCustomQuery([](std::string_view method, KrpcQuery const& query,
                       IpEndpoint const&) {
    if (method != "bx_custom") {
      return std::optional<blocxxi::codec::bencode::Value::DictionaryType> {};
    }
    return std::optional<blocxxi::codec::bencode::Value::DictionaryType> {
      blocxxi::codec::bencode::Value::DictionaryType {
        { "echo", blocxxi::codec::bencode::Value(query.arguments_.at("key").AsString()) },
      },
    };
  });
  node.Start();

  auto custom_reply_seen = false;
  sender->AsyncReceive([&custom_reply_seen](std::error_code const& failure,
                        IpEndpoint const&, BufferReader const& buffer) {
    ASSERT_FALSE(failure);
    auto message = DecodeKrpc(std::string_view(
      reinterpret_cast<char const*>(buffer.data()), buffer.size()));
    ASSERT_EQ(GetType(message), KrpcMessage::Type::Response);
    auto const& response = std::get<KrpcResponse>(message.payload_);
    EXPECT_EQ(response.values_.at("echo").AsString(), "custom-key");
    EXPECT_TRUE(response.values_.contains("id"));
    custom_reply_seen = true;
  });

  sender->AsyncSend(MakeCustomQuery("zz", "bx_custom"),
    IpEndpoint { "127.0.0.1", 30131 },
    [](std::error_code const& failure) { ASSERT_FALSE(failure); });
  io_context.run_for(std::chrono::seconds(1));
  ASSERT_TRUE(custom_reply_seen);
}

} // namespace blocxxi::p2p::kademlia
NOVA_DIAGNOSTIC_POP
