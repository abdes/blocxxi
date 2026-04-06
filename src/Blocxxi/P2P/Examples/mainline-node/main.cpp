//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <Blocxxi/P2P/kademlia/channel.h>
#include <Blocxxi/P2P/kademlia/krpc.h>
#include <Blocxxi/P2P/kademlia/session.h>
#include <Blocxxi/P2P/kademlia/node.h>

namespace {

using blocxxi::p2p::kademlia::AsyncUdpChannel;
using blocxxi::p2p::kademlia::DecodeCompactPeer;
using blocxxi::p2p::kademlia::DecodeKrpc;
using blocxxi::p2p::kademlia::GetType;
using blocxxi::p2p::kademlia::KrpcMessage;
using blocxxi::p2p::kademlia::KrpcResponse;
using blocxxi::p2p::kademlia::MainlineDhtNode;
using blocxxi::p2p::kademlia::Node;
using blocxxi::p2p::kademlia::Session;

struct Args {
  std::string bind_address_ { "127.0.0.1" };
  std::uint16_t port_ { 0 };
  std::chrono::milliseconds duration_ { 15000 };
  std::optional<Node::IdType> node_id_;
  std::vector<std::string> bootstrap_nodes_;
  std::optional<std::string> remote_;
  std::optional<std::string> query_;
  std::optional<Node::IdType> target_;
  std::optional<std::string> token_;
  std::uint16_t announce_port_ { 0 };
  bool implied_port_ { false };
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
    } else if (current == "--remote") {
      args.remote_ = std::string(require_value(current));
    } else if (current == "--query") {
      args.query_ = std::string(require_value(current));
    } else if (current == "--target") {
      args.target_ = Node::IdType::FromHex(std::string(require_value(current)));
    } else if (current == "--token") {
      args.token_ = std::string(require_value(current));
    } else if (current == "--announce-port") {
      args.announce_port_
        = static_cast<std::uint16_t>(std::stoul(std::string(require_value(current))));
    } else if (current == "--implied-port") {
      args.implied_port_ = true;
    } else {
      throw std::invalid_argument("unknown argument: " + std::string(current));
    }
  }
  return args;
}

auto ParseEndpoint(std::string const& endpoint)
  -> blocxxi::p2p::kademlia::IpEndpoint
{
  auto const separator = endpoint.rfind(':');
  if (separator == std::string::npos) {
    throw std::invalid_argument("bad endpoint: " + endpoint);
  }

  auto const host = endpoint.substr(0, separator);
  auto const port = static_cast<std::uint16_t>(
    std::stoul(endpoint.substr(separator + 1)));
  return { host, port };
}

void PrintResponse(KrpcMessage const& response)
{
  std::cout << "CLIENT_RESPONSE type=";
  switch (GetType(response)) {
  case blocxxi::p2p::kademlia::KrpcMessage::Type::Query:
    std::cout << "query";
    break;
  case blocxxi::p2p::kademlia::KrpcMessage::Type::Response:
    std::cout << "response";
    break;
  case blocxxi::p2p::kademlia::KrpcMessage::Type::Error:
    std::cout << "error";
    break;
  }
  std::cout << " tx=" << response.transaction_id_ << std::endl;

  if (GetType(response) != blocxxi::p2p::kademlia::KrpcMessage::Type::Response) {
    return;
  }

  auto const& body = std::get<KrpcResponse>(response.payload_).values_;
  if (body.contains("token")) {
    std::cout << "CLIENT_TOKEN " << body.at("token").AsString() << std::endl;
  }
  if (body.contains("values")) {
    for (auto const& value : body.at("values").AsList()) {
      auto peer = DecodeCompactPeer(value.AsString());
      std::cout << "CLIENT_PEER " << peer.Address().to_string() << ":"
                << peer.Port() << std::endl;
    }
  }
  if (body.contains("samples")) {
    std::cout << "CLIENT_SAMPLES " << body.at("samples").AsString().size()
              << std::endl;
  }
}

} // namespace

auto main(int argc, char** argv) -> int
{
  try {
    auto const args = ParseArgs(argc, argv);

    auto io_context = asio::io_context {};
    auto channel = AsyncUdpChannel::ipv4(
      io_context, args.bind_address_, std::to_string(args.port_));
    auto timer = asio::steady_timer(io_context);

    auto const endpoint = channel->LocalEndpoint();
    auto self = Node(args.node_id_.value_or(Node::IdType::RandomHash()),
      endpoint.Address().to_string(), endpoint.Port());

    std::cout << "READY " << self.Endpoint().Address().to_string() << ":"
              << self.Endpoint().Port() << " " << self.Id().ToHex()
              << std::endl;

    auto session
      = Session(MainlineDhtNode(io_context, std::move(self),
          std::move(channel)));
    session.OnQuery([](std::string_view method,
                   blocxxi::p2p::kademlia::IpEndpoint const& sender) {
      std::cout << "QUERY " << method << " " << sender.Address().to_string()
                << ":" << sender.Port() << std::endl;
    });
    session.OnBootstrapSuccess([](std::string const& bootstrap) {
      std::cout << "BOOTSTRAP_OK " << bootstrap << std::endl;
    });
    for (auto const& bootstrap : args.bootstrap_nodes_) {
      session.AddBootstrapNode(bootstrap);
    }
    session.Start();

    if (args.remote_ && args.query_) {
      auto const remote = ParseEndpoint(*args.remote_);
      auto const target = args.target_.value_or(self.Id());

      auto on_complete = [&](std::error_code const& failure,
                            KrpcMessage const& response) {
        if (failure) {
          std::cout << "CLIENT_ERROR " << failure.message() << std::endl;
        } else {
          PrintResponse(response);
        }
        session.Stop();
        io_context.stop();
      };

      if (*args.query_ == "ping") {
        session.AsyncPing(remote, on_complete);
      } else if (*args.query_ == "find_node") {
        session.AsyncFindNode(target, remote, on_complete);
      } else if (*args.query_ == "get_peers") {
        session.AsyncGetPeers(target, remote, on_complete);
      } else if (*args.query_ == "sample_infohashes") {
        session.AsyncSampleInfohashes(target, remote, on_complete);
      } else if (*args.query_ == "announce_peer") {
        if (!args.token_) {
          throw std::invalid_argument("--token is required for announce_peer");
        }
        session.AsyncAnnouncePeer(target, *args.token_, args.announce_port_,
          args.implied_port_, remote, on_complete);
      } else {
        throw std::invalid_argument("unknown query mode: " + *args.query_);
      }
    }

    timer.expires_after(args.duration_);
    timer.async_wait([&](std::error_code const&) {
      session.Stop();
      io_context.stop();
    });

    io_context.run();
    return EXIT_SUCCESS;
  } catch (std::exception const& ex) {
    std::cerr << "error: " << ex.what() << std::endl;
    return EXIT_FAILURE;
  }
}
