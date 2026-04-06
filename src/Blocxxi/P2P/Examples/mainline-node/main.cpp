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
#include <string_view>
#include <vector>

#include <Blocxxi/P2P/kademlia/channel.h>
#include <Blocxxi/P2P/kademlia/mainline_node.h>
#include <Blocxxi/P2P/kademlia/node.h>

namespace {

using blocxxi::p2p::kademlia::AsyncUdpChannel;
using blocxxi::p2p::kademlia::MainlineDhtNode;
using blocxxi::p2p::kademlia::Node;

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

    auto node = MainlineDhtNode(io_context, std::move(self), std::move(channel));
    node.OnQuery([](std::string_view method,
                   blocxxi::p2p::kademlia::IpEndpoint const& sender) {
      std::cout << "QUERY " << method << " " << sender.Address().to_string()
                << ":" << sender.Port() << std::endl;
    });
    node.OnBootstrapSuccess([](std::string const& bootstrap) {
      std::cout << "BOOTSTRAP_OK " << bootstrap << std::endl;
    });
    for (auto const& bootstrap : args.bootstrap_nodes_) {
      node.AddBootstrapNode(bootstrap);
    }
    node.Start();

    timer.expires_after(args.duration_);
    timer.async_wait([&](std::error_code const&) {
      node.Stop();
      io_context.stop();
    });

    io_context.run();
    return EXIT_SUCCESS;
  } catch (std::exception const& ex) {
    std::cerr << "error: " << ex.what() << std::endl;
    return EXIT_FAILURE;
  }
}
