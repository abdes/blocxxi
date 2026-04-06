//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Nova/Base/Compilers.h>

#include <Blocxxi/Nat/nat.h>
#include <Blocxxi/P2P/kademlia/channel.h>
#include <Blocxxi/P2P/kademlia/engine.h>
#include <Blocxxi/P2P/kademlia/message_serializer.h>
#include <Blocxxi/P2P/kademlia/session.h>
#include <Nova/Base/Logging.h>

#include "../utils/console_runner.h"

#include <charconv>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace kad = blocxxi::p2p::kademlia;

using blocxxi::nat::PortMapper;
using kad::AsyncUdpChannel;
using kad::Engine;
using kad::KeyType;
using kad::MessageSerializer;
using kad::Network;
using kad::Node;
using kad::RoutingTable;
using kad::Session;

using NetworkType = kad::Network<AsyncUdpChannel, MessageSerializer>;
using EngineType = kad::Engine<RoutingTable, NetworkType>;

namespace {

struct CommandLineOptions {
  std::string nat_spec { "upnp" };
  std::string ipv6_address { "::1" };
  std::uint16_t port { 9000 };
  std::vector<std::string> boot_list;
  bool show_debug_gui { false };
  bool help { false };
};

constexpr auto kHelpText = R"(Allowed options:
  --help
      produce help message
  --nat, -n <spec>
      NAT specification as one of the following accepted formats:
      'upnp','extip:1.2.3.4[:192.168.1.5]'. If not present, best effort
      selection of the address will be automatically done.
  --ipv6 <address>
      specify an IPV6 address to bind to as well.
  --port, -p <port>
      port number
  --bootstrap, -b <node-url>
      bootstrap peer URL. Repeat the option to provide multiple peers.
  --debug-ui, -d [true|false]
      show the debug UI
)";

auto JoinStrings(std::span<std::string const> values,
  std::string_view separator) -> std::string
{
  if (values.empty()) {
    return {};
  }

  auto joined = values.front();
  for (auto index = std::size_t { 1 }; index < values.size(); ++index) {
    joined.append(separator);
    joined.append(values[index]);
  }
  return joined;
}

auto ParseBool(std::string_view value) -> bool
{
  if (value == "1" || value == "true" || value == "TRUE" || value == "True"
    || value == "yes" || value == "on") {
    return true;
  }
  if (value == "0" || value == "false" || value == "FALSE" || value == "False"
    || value == "no" || value == "off") {
    return false;
  }
  throw std::invalid_argument("invalid boolean value: " + std::string(value));
}

auto ParsePort(std::string_view value) -> std::uint16_t
{
  unsigned int parsed = 0;
  const auto* begin = value.data();
  const auto* end = begin + value.size();
  const auto [ptr, error] = std::from_chars(begin, end, parsed);
  if (error != std::errc {} || ptr != end
    || parsed > std::numeric_limits<std::uint16_t>::max()) {
    throw std::invalid_argument("invalid port value: " + std::string(value));
  }
  return static_cast<std::uint16_t>(parsed);
}

auto RequireValue(int& index, int argc, char** argv,
  std::string_view option_name) -> std::string_view
{
  ++index;
  if (index >= argc) {
    throw std::invalid_argument(
      "missing value for option " + std::string(option_name));
  }
  return argv[index];
}

auto ParseCommandLine(int argc, char** argv) -> CommandLineOptions
{
  auto options = CommandLineOptions {};

  for (auto index = 1; index < argc; ++index) {
    const auto arg = std::string_view(argv[index]);

    if (arg == "--help") {
      options.help = true;
      continue;
    }
    if (arg == "--nat" || arg == "-n") {
      options.nat_spec = RequireValue(index, argc, argv, arg);
      continue;
    }
    if (arg.starts_with("--nat=")) {
      options.nat_spec = std::string(arg.substr(6));
      continue;
    }
    if (arg == "--ipv6") {
      options.ipv6_address = RequireValue(index, argc, argv, arg);
      continue;
    }
    if (arg.starts_with("--ipv6=")) {
      options.ipv6_address = std::string(arg.substr(7));
      continue;
    }
    if (arg == "--port" || arg == "-p") {
      options.port = ParsePort(RequireValue(index, argc, argv, arg));
      continue;
    }
    if (arg.starts_with("--port=")) {
      options.port = ParsePort(arg.substr(7));
      continue;
    }
    if (arg == "--bootstrap" || arg == "-b") {
      options.boot_list.emplace_back(RequireValue(index, argc, argv, arg));
      continue;
    }
    if (arg.starts_with("--bootstrap=")) {
      options.boot_list.emplace_back(arg.substr(12));
      continue;
    }
    if (arg == "--debug-ui" || arg == "-d") {
      const auto next_is_value = (index + 1 < argc)
        && !std::string_view(argv[index + 1]).starts_with('-');
      options.show_debug_gui = next_is_value
        ? ParseBool(RequireValue(index, argc, argv, arg))
        : true;
      continue;
    }
    if (arg.starts_with("--debug-ui=")) {
      options.show_debug_gui = ParseBool(arg.substr(11));
      continue;
    }

    throw std::invalid_argument("unknown option: " + std::string(arg));
  }

  return options;
}

} // namespace

inline void Shutdown() { LOG_F(INFO, "Shutdown complete"); }

auto main(int argc, char** argv) -> int
{
  try {
    auto options = ParseCommandLine(argc, argv);
    if (options.help) {
      std::cout << kHelpText;
      return 0;
    }

    (void)options.show_debug_gui;

    if (!options.boot_list.empty()) {
      LOG_F(INFO, "Node will bootstrap from {} peer(s): {}",
        options.boot_list.size(), JoinStrings(options.boot_list, ", "));
    } else {
      LOG_F(INFO, "Node starting as a bootstrap node");
    }

    auto mapper = blocxxi::nat::GetPortMapper(options.nat_spec);
    if (!mapper) {
      LOG_F(ERROR, "Failed to initialize the network.");
      LOG_F(ERROR,
        "Try to explicitly set the local and external addresses "
        "using the 'extip:' NAT spec.");
      return -1;
    }
    mapper->AddMapping({ PortMapper::Protocol::UDP, options.port, options.port,
                         "simple-node kademlia" },
      std::chrono::seconds(0));

    asio::io_context io_context;

    auto my_node
      = Node { Node::IdType::RandomHash(), mapper->ExternalIP(), options.port };
    auto routing_table = RoutingTable { my_node, kad::CONCURRENCY_K };

    auto ipv4 = AsyncUdpChannel::ipv4(io_context, mapper->InternalIP(),
      std::to_string(static_cast<unsigned int>(options.port)));

    auto ipv6 = AsyncUdpChannel::ipv6(io_context, options.ipv6_address,
      std::to_string(static_cast<unsigned int>(options.port)));

    auto serializer = std::make_unique<MessageSerializer>(my_node.Id());
    auto network = NetworkType { io_context, std::move(serializer),
      std::move(ipv4), std::move(ipv6) };

    auto engine
      = EngineType { io_context, std::move(routing_table), std::move(network) };
    for (auto const& bootstrap_node : options.boot_list) {
      engine.AddBootstrapNode(bootstrap_node);
    }

    using SessionType = Session<EngineType>;
    auto session = SessionType(io_context, std::move(engine));
    session.Start();

    auto server_thread = std::thread([&io_context]() { io_context.run(); });

    if (!options.boot_list.empty()) {
      auto value = SessionType::DataType { 0x01, 0x02 };
      auto key = KeyType::RandomHash();
      session.StoreValue(key, value,
        [&session, &value, &key](std::error_code const& store_errc) {
          if (store_errc) {
            LOG_F(ERROR, "store value failed: {}", store_errc.message());
          } else {
            value.clear();
            session.FindValue(key,
              [](std::error_code const& find_errc,
                SessionType::DataType const& search_value) {
                if (find_errc) {
                  LOG_F(ERROR, "find value failed: {}", find_errc.message());
                } else {
                  LOG_F(INFO, "value: {} {}", search_value[0], search_value[1]);
                }
              });
          }
        });
    }

    LOG_F(INFO, "starting in console mode...");
    auto runner = blocxxi::ConsoleRunner(Shutdown);
    runner.Run();

    LOG_F(INFO, "Shutting down...");
    mapper->DeleteMapping(PortMapper::Protocol::UDP, options.port);
    io_context.stop();
    server_thread.join();
  } catch (std::exception const& error) {
    LOG_F(ERROR, "Error: {}", error.what());
    return -1;
  } catch (...) {
    LOG_F(ERROR, "Unknown error!");
    return -1;
  }

  return 0;
}
