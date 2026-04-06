//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <Blocxxi/P2P/kademlia/asio.h>
#include <Blocxxi/P2P/kademlia/krpc.h>
#include <Blocxxi/P2P/kademlia/node.h>

namespace {

using blocxxi::p2p::kademlia::DecodeKrpc;
using blocxxi::p2p::kademlia::Encode;
using blocxxi::p2p::kademlia::EncodeCompactNode;
using blocxxi::p2p::kademlia::IpEndpoint;
using blocxxi::p2p::kademlia::KrpcMessage;
using blocxxi::p2p::kademlia::KrpcQuery;
using blocxxi::p2p::kademlia::KrpcResponse;
using blocxxi::p2p::kademlia::Node;

auto HashToBytes(Node::IdType const& id) -> std::string
{
  return std::string(reinterpret_cast<char const*>(id.Data()), id.Size());
}

struct Args {
  std::string bind_address_ { "127.0.0.1" };
  std::uint16_t port_ { 0 };
  std::chrono::milliseconds duration_ { 15000 };
  std::optional<Node::IdType> node_id_;
  std::vector<std::string> bootstrap_nodes_;
};

auto ParseArgs(int argc, char** argv) -> Args
{
  auto args = Args {};
  for (auto index = 1; index < argc; ++index) {
    auto const current = std::string_view(argv[index]);
    auto require_value = [&](std::string_view flag) -> std::string_view {
      if (index + 1 >= argc) {
        throw std::invalid_argument("missing value for " + std::string(flag));
      }
      ++index;
      return argv[index];
    };

    if (current == "--bind-address") {
      args.bind_address_ = std::string(require_value(current));
    } else if (current == "--port") {
      args.port_ = static_cast<std::uint16_t>(
        std::stoul(std::string(require_value(current))));
    } else if (current == "--duration-ms") {
      args.duration_ = std::chrono::milliseconds(
        std::stoll(std::string(require_value(current))));
    } else if (current == "--node-id") {
      args.node_id_ = Node::IdType::FromHex(std::string(require_value(current)));
    } else if (current == "--bootstrap") {
      args.bootstrap_nodes_.emplace_back(require_value(current));
    } else {
      throw std::invalid_argument("unknown argument: " + std::string(current));
    }
  }
  return args;
}

auto MakeResponse(
  std::string_view method, Node const& self, std::string transaction_id)
  -> std::optional<KrpcMessage>
{
  auto values = blocxxi::codec::bencode::Value::DictionaryType {
    { "id", blocxxi::codec::bencode::Value(HashToBytes(self.Id())) },
  };

  if (method == "find_node") {
    values.emplace("nodes", blocxxi::codec::bencode::Value(EncodeCompactNode(self)));
  } else if (method == "get_peers") {
    values.emplace("nodes", blocxxi::codec::bencode::Value(EncodeCompactNode(self)));
    values.emplace("token", blocxxi::codec::bencode::Value("blocxxi-token"));
  } else if (method == "announce_peer" || method == "ping") {
    // Only the id field is required for this minimal fixture response.
  } else {
    return std::nullopt;
  }

  auto response = KrpcMessage {};
  response.transaction_id_ = std::move(transaction_id);
  response.version_ = "BX01";
  response.payload_ = KrpcResponse { std::move(values) };
  return response;
}

auto MakeBootstrapQuery(Node const& self, std::string transaction_id)
  -> KrpcMessage
{
  auto query = KrpcMessage {};
  query.transaction_id_ = std::move(transaction_id);
  query.version_ = "BX01";
  query.payload_ = KrpcQuery {
    "find_node",
    {
      { "id", blocxxi::codec::bencode::Value(HashToBytes(self.Id())) },
      { "target", blocxxi::codec::bencode::Value(HashToBytes(self.Id())) },
    },
  };
  return query;
}

void SendBootstrapQueries(asio::io_context& io_context,
  asio::ip::udp::socket& socket, Node const& self,
  std::vector<std::string> const& bootstrap_nodes,
  std::unordered_map<std::string, std::string>& pending_bootstraps)
{
  auto resolver = asio::ip::udp::resolver(io_context);
  auto tx_index = 0U;
  for (auto const& bootstrap : bootstrap_nodes) {
    auto const separator = bootstrap.rfind(':');
    if (separator == std::string::npos) {
      throw std::invalid_argument("bad bootstrap endpoint: " + bootstrap);
    }

    auto const host = bootstrap.substr(0, separator);
    auto const service = bootstrap.substr(separator + 1);
    auto resolved = resolver.resolve(asio::ip::udp::v4(), host, service);
    auto const destination = *resolved.begin();

    auto transaction_id
      = std::string("b") + static_cast<char>('0' + (tx_index % 10U));
    ++tx_index;

    pending_bootstraps.emplace(transaction_id, bootstrap);
    auto encoded = Encode(MakeBootstrapQuery(self, transaction_id));
    socket.async_send_to(asio::buffer(encoded), destination.endpoint(),
      [encoded = std::move(encoded)](std::error_code const&, std::size_t) { });
  }
}

} // namespace

auto main(int argc, char** argv) -> int
{
  try {
    auto const args = ParseArgs(argc, argv);

    auto io_context = asio::io_context {};
    auto socket = asio::ip::udp::socket(io_context,
      asio::ip::udp::endpoint(
        asio::ip::make_address(args.bind_address_), args.port_));
    auto timer = asio::steady_timer(io_context);

    auto const endpoint = socket.local_endpoint();
    auto self = Node(args.node_id_.value_or(Node::IdType::RandomHash()),
      endpoint.address().to_string(), endpoint.port());

    std::cout << "READY " << self.Endpoint().Address().to_string() << ":"
              << self.Endpoint().Port() << " " << self.Id().ToHex()
              << std::endl;

    auto receive_buffer = std::make_shared<std::array<std::uint8_t, 2048>>();
    auto sender = std::make_shared<asio::ip::udp::endpoint>();
    auto pending_bootstraps
      = std::make_shared<std::unordered_map<std::string, std::string>>();

    std::shared_ptr<std::function<void()>> receive_loop
      = std::make_shared<std::function<void()>>();
    *receive_loop = [&]() {
      socket.async_receive_from(asio::buffer(*receive_buffer), *sender,
        [&, receive_buffer, sender, receive_loop, pending_bootstraps](
          std::error_code const& failure, std::size_t bytes_received) {
          if (!failure) {
            try {
              auto message = DecodeKrpc(std::string_view(
                reinterpret_cast<char const*>(receive_buffer->data()),
                bytes_received));

              if (std::holds_alternative<KrpcQuery>(message.payload_)) {
                auto const& query = std::get<KrpcQuery>(message.payload_);
                std::cout << "QUERY " << query.method_ << " "
                          << sender->address().to_string() << ":"
                          << sender->port() << std::endl;
                if (auto response = MakeResponse(
                      query.method_, self, message.transaction_id_)) {
                  auto encoded = Encode(*response);
                  socket.async_send_to(asio::buffer(encoded), *sender,
                    [encoded = std::move(encoded)](
                      std::error_code const&, std::size_t) { });
                }
              } else if (pending_bootstraps->contains(message.transaction_id_)) {
                std::cout << "BOOTSTRAP_OK "
                          << pending_bootstraps->at(message.transaction_id_)
                          << std::endl;
                pending_bootstraps->erase(message.transaction_id_);
              }
            } catch (...) {
              // Ignore malformed packets in this minimal fixture.
            }
          }

          if (!failure || failure == asio::error::message_size) {
            (*receive_loop)();
          }
        });
    };

    (*receive_loop)();

    SendBootstrapQueries(
      io_context, socket, self, args.bootstrap_nodes_, *pending_bootstraps);

    timer.expires_after(args.duration_);
    timer.async_wait([&](std::error_code const&) { io_context.stop(); });

    io_context.run();
    return EXIT_SUCCESS;
  } catch (std::exception const& ex) {
    std::cerr << "error: " << ex.what() << std::endl;
    return EXIT_FAILURE;
  }
}
