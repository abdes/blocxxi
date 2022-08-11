//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <common/compilers.h>

ASAP_DIAGNOSTIC_PUSH
#if defined(ASAP_GNUC_VERSION)
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wredundant-decls"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#include <boost/algorithm/string/join.hpp> // for string join
#include <boost/program_options.hpp>
#include <boost/thread.hpp>
ASAP_DIAGNOSTIC_POP

#include <logging/logging.h>
#include <nat/nat.h>
#include <p2p/kademlia/engine.h>
#include <p2p/kademlia/session.h>

#include "../utils/console_runner.h"

#include <iostream>
#include <utility> // for std::move

namespace bpo = boost::program_options;
namespace kad = blocxxi::p2p::kademlia;

using blocxxi::nat::PortMapper;
using blocxxi::p2p::kademlia::AsyncUdpChannel;
using blocxxi::p2p::kademlia::Channel;
using blocxxi::p2p::kademlia::Engine;
using blocxxi::p2p::kademlia::KeyType;
using blocxxi::p2p::kademlia::MessageSerializer;
using blocxxi::p2p::kademlia::Network;
using blocxxi::p2p::kademlia::Node;
using blocxxi::p2p::kademlia::ResponseDispatcher;
using blocxxi::p2p::kademlia::RoutingTable;
using blocxxi::p2p::kademlia::Session;

using NetworkType =
    blocxxi::p2p::kademlia::Network<AsyncUdpChannel, MessageSerializer>;
using EngineType = blocxxi::p2p::kademlia::Engine<RoutingTable, NetworkType>;

inline void Shutdown() {
  auto &logger = asap::logging::Registry::GetLogger("simple-node");
  // Shutdown
  ASLOG_TO_LOGGER(logger, info, "Shutdown complete");
}

auto main(int argc, char **argv) -> int {
  auto &logger = asap::logging::Registry::GetLogger("simple-node");

  constexpr uint16_t c_default_port = 9000;

  boost::thread server_thread;
  std::string nat_spec;
  std::string ipv6_address;
  uint16_t port = 0;
  std::vector<std::string> boot_list;
  bool show_debug_gui{false};
  try {
    // Command line arguments
    bpo::options_description desc("Allowed options");
    // clang-format off
    desc.add_options()
      ("help", "produce help message")
      ("nat,n", bpo::value<std::string>(&nat_spec)->default_value("upnp"),
        "NAT specification as one of the following accepted formats: "
        "'upnp','extip:1.2.3.4[:192.168.1.5]'. If not present, best effort "
        "selection of the address will be automatically done.")
      ("ipv6", bpo::value<std::string>(&ipv6_address)->default_value("::1"),
        "specify an IPV6 address to bind to as well.")
      ("port,p", bpo::value<uint16_t>(&port)->default_value(c_default_port),
        "port number")
      ("bootstrap,b",bpo::value<std::vector<std::string>>(&boot_list))
      ("debug-ui,d", bpo::value<bool>(&show_debug_gui)->default_value(false),
        "show the debug UI");
    // clang-format on

    bpo::variables_map bpo_vm;
    bpo::store(bpo::parse_command_line(argc, argv, desc), bpo_vm);

    if (bpo_vm.count("help") != 0) {
      std::cout << desc << std::endl;
      return 0;
    }

    bpo::notify(bpo_vm);

    // TODO(Abdessattar): add custom validator for ip address
    // TODO(Abdessattar): add custom validator for bootstrap node

    if (!boot_list.empty()) {
      ASLOG_TO_LOGGER(logger, info, "Node will bootstrap from {} peer(s): {}",
          boot_list.size(), boost::algorithm::join(boot_list, ", "));
    } else {
      ASLOG_TO_LOGGER(logger, info, "Node starting as a bootstrap node");
    }

    //
    // Get a suitable port mapper and set it up.
    //
    auto mapper = blocxxi::nat::GetPortMapper(nat_spec);
    if (!mapper) {
      ASLOG_TO_LOGGER(logger, error, "Failed to initialize the network.");
      ASLOG_TO_LOGGER(logger, error,
          "Try to explicitly set the local and external addresses "
          "using the 'extip:' NAT spec.");
      return -1;
    }
    mapper->AddMapping(PortMapper::Protocol::UDP, port, port,
        "ndagent kademlia", std::chrono::seconds(0));

    boost::asio::io_context io_context;

    auto my_node = Node{Node::IdType::RandomHash(), mapper->ExternalIP(), port};
    auto routing_table =
        RoutingTable{my_node, blocxxi::p2p::kademlia::CONCURRENCY_K};

    auto ipv4 = AsyncUdpChannel::ipv4(io_context, mapper->InternalIP(),
        std::to_string(static_cast<unsigned int>(port)));

    auto ipv6 = AsyncUdpChannel::ipv6(io_context, ipv6_address,
        std::to_string(static_cast<unsigned int>(port)));

    auto serializer = std::make_unique<MessageSerializer>(my_node.Id());
    auto network = NetworkType{
        io_context, std::move(serializer), std::move(ipv4), std::move(ipv6)};

    auto engine =
        EngineType{io_context, std::move(routing_table), std::move(network)};
    for (auto const &bnurl : boot_list) {
      engine.AddBootstrapNode(bnurl);
    }

    using SessionType = Session<EngineType>;
    auto session = SessionType(io_context, std::move(engine));
    session.Start();

    server_thread = boost::thread([&io_context]() { io_context.run(); });

    if (!boot_list.empty()) {
      //
      // STORE a value
      //
      auto value = SessionType::DataType{0x01, 0x02};
      auto key = KeyType::RandomHash();
      session.StoreValue(key, value,
          [&logger, &session, &value, &key](std::error_code const &store_errc) {
            if (store_errc) {
              ASLOG_TO_LOGGER(logger, error, "store value failed: {}",
                  store_errc.message());
            } else {
              //
              // FIND the value
              //
              value.clear();
              session.FindValue(
                  key, [&logger](std::error_code const &find_errc,
                           SessionType::DataType const &search_value) {
                    if (find_errc) {
                      ASLOG_TO_LOGGER(logger, error, "find value failed: {}",
                          find_errc.message());
                    } else {
                      ASLOG_TO_LOGGER(logger, info, "value: {} {}",
                          search_value[0], search_value[1]);
                    }
                  });
            }
          });
    }

    ASLOG_TO_LOGGER(logger, info, "starting in console mode...");
    //
    // Start the console runner
    //
    blocxxi::ConsoleRunner runner(Shutdown);
    runner.Run();

    // Shutdown
    ASLOG_TO_LOGGER(logger, info, "Shutting down...");
    mapper->DeleteMapping(PortMapper::Protocol::UDP, port);
    io_context.stop();
    server_thread.join();

  } catch (std::exception &e) {
    ASLOG_TO_LOGGER(logger, error, "Error: {}", e.what());
    return -1;
  } catch (...) {
    ASLOG_TO_LOGGER(logger, error, "Unknown error!");
    return -1;
  }

  return 0;
}
