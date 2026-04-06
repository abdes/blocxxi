//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <Blocxxi/P2P/kademlia/channel.h>
#include <Blocxxi/P2P/kademlia/krpc.h>
#include <Blocxxi/P2P/kademlia/session.h>
#include <Blocxxi/P2P/kademlia/node.h>

namespace {

using blocxxi::p2p::kademlia::AsyncUdpChannel;
using blocxxi::p2p::kademlia::DecodeCompactNode;
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
  bool dedupe_output_ { true };
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
    } else if (current == "--no-dedupe") {
      args.dedupe_output_ = false;
    } else {
      throw std::invalid_argument("unknown argument: " + std::string(current));
    }
  }
  return args;
}

auto ParseEndpoint(asio::io_context& io_context, std::string const& endpoint)
  -> blocxxi::p2p::kademlia::IpEndpoint
{
  auto const separator = endpoint.rfind(':');
  if (separator == std::string::npos) {
    throw std::invalid_argument("bad endpoint: " + endpoint);
  }

  auto const host = endpoint.substr(0, separator);
  auto const service = endpoint.substr(separator + 1);
  auto endpoints = AsyncUdpChannel::ResolveEndpoint(io_context, host, service);
  if (endpoints.empty()) {
    throw std::invalid_argument("failed to resolve endpoint: " + endpoint);
  }

  return endpoints.front();
}

void PrintResponse(KrpcMessage const& response, bool dedupe_output)
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
    auto seen = std::set<std::string> {};
    for (auto const& value : body.at("values").AsList()) {
      auto peer = DecodeCompactPeer(value.AsString());
      auto const key
        = peer.Address().to_string() + ":" + std::to_string(peer.Port());
      if (!dedupe_output || seen.insert(key).second) {
        std::cout << "CLIENT_PEER " << key << std::endl;
      }
    }
  }
  if (body.contains("nodes")) {
    auto const& nodes = body.at("nodes").AsString();
    std::cout << "CLIENT_NODES_BYTES " << nodes.size() << std::endl;
    auto seen = std::set<std::string> {};
    for (std::size_t offset = 0; offset + 26 <= nodes.size(); offset += 26) {
      auto node = DecodeCompactNode(nodes.substr(offset, 26));
      auto const key = node.Endpoint().Address().to_string() + ":"
        + std::to_string(node.Endpoint().Port()) + " " + node.Id().ToHex();
      if (!dedupe_output || seen.insert(key).second) {
        std::cout << "CLIENT_NODE " << key << std::endl;
      }
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
      auto const remote = ParseEndpoint(io_context, *args.remote_);
      auto const target = args.target_.value_or(self.Id());

      auto on_complete = [&](std::error_code const& failure,
                            KrpcMessage const& response) {
        if (failure) {
          std::cout << "CLIENT_ERROR " << failure.message() << std::endl;
        } else {
          PrintResponse(response, args.dedupe_output_);
        }
        asio::post(io_context, [&session, &io_context]() {
          session.Stop();
          io_context.stop();
        });
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
      } else if (*args.query_ == "discover") {
        struct DiscoveryState {
          std::set<std::string> seen_keys_;
          std::deque<blocxxi::p2p::kademlia::IpEndpoint> pending_;
          std::set<std::string> queued_endpoints_;
          std::size_t outstanding_ { 0 };
          std::size_t total_unique_ { 0 };
        };

        auto state = std::make_shared<DiscoveryState>();
        auto enqueue_endpoint = [&](blocxxi::p2p::kademlia::IpEndpoint endpoint) {
          auto const key = endpoint.ToString();
          if (state->queued_endpoints_.insert(key).second) {
            state->pending_.push_back(std::move(endpoint));
          }
        };
        enqueue_endpoint(remote);
        for (auto const& bootstrap : args.bootstrap_nodes_) {
          enqueue_endpoint(ParseEndpoint(io_context, bootstrap));
        }

        auto schedule = std::make_shared<std::function<void()>>();
        *schedule = [&, state, schedule, target]() {
          constexpr auto kConcurrency = std::size_t { 8 };
          while (state->outstanding_ < kConcurrency && !state->pending_.empty()) {
            auto endpoint = state->pending_.front();
            state->pending_.pop_front();
            ++state->outstanding_;
            session.AsyncFindNode(target, endpoint,
              [&, state, schedule](std::error_code const& failure,
                KrpcMessage const& response) {
                if (!failure
                  && GetType(response)
                    == blocxxi::p2p::kademlia::KrpcMessage::Type::Response) {
                  auto const& values
                    = std::get<KrpcResponse>(response.payload_).values_;
                  if (values.contains("nodes")) {
                    auto const& nodes = values.at("nodes").AsString();
                    for (std::size_t offset = 0; offset + 26 <= nodes.size();
                         offset += 26) {
                      auto node = DecodeCompactNode(nodes.substr(offset, 26));
                      auto const key = node.Endpoint().Address().to_string()
                        + ":" + std::to_string(node.Endpoint().Port()) + " "
                        + node.Id().ToHex();
                      if (state->seen_keys_.insert(key).second) {
                        ++state->total_unique_;
                        std::cout << "DISCOVERED_NODE " << key << std::endl;
                        std::cout << "DISCOVERY_TOTAL " << state->total_unique_
                                  << std::endl;
                        auto endpoint_key = node.Endpoint().ToString();
                        if (state->queued_endpoints_.insert(endpoint_key).second) {
                          state->pending_.push_back(node.Endpoint());
                        }
                      }
                    }
                  }
                }

                if (state->outstanding_ > 0) {
                  --state->outstanding_;
                }
                (*schedule)();
              });
          }
        };
        (*schedule)();
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
