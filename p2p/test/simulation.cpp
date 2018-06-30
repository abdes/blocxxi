//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <iostream>

#include <boost/program_options.hpp>

#include <common/logging.h>
#include <p2p/kademlia/routing.h>

namespace bpo = boost::program_options;

using blocxxi::p2p::kademlia::Node;
using blocxxi::p2p::kademlia::RoutingTable;

int main(int argc, char** argv) {
  ASLOG_MISC(info, "Simulation program started");

  auto name = std::string("Stranger");
  try {
    // Command line arguments
    bpo::options_description desc("Allowed options");
    desc.add_options()("help", "produce help message")(
        "name,n", bpo::value<std::string>(&name), "person to greet");

    bpo::variables_map bpo_vm;
    bpo::store(bpo::parse_command_line(argc, argv, desc), bpo_vm);

    if (bpo_vm.count("help")) {
      std::cout << desc << std::endl;
      return 0;
    }

    bpo::notify(bpo_vm);
    std::cout << "Hello " << name << std::endl;

    // ---------------------------------------------------- //

    auto my_node_id_hex = "FA631C8E7655EF7BC6E99D8A3BF810E21428BFD4";
    auto my_node_id = Node::IdType::FromHex(my_node_id_hex);
    auto my_node = Node(my_node_id, "127.0.0.1", 3030);

    constexpr auto const KSIZE = 20;
    RoutingTable rt(std::move(my_node), KSIZE);

    ASLOG_MISC(info, "Adding {} nodes...", KSIZE);
    for (int ii = 0; ii < KSIZE + 1; ++ii) {
      rt.AddPeer(Node(Node::IdType::RandomHash(), "::1", 3030));
    }

    for (int ii = 0; ii < 200; ++ii) {
      rt.AddPeer(Node(Node::IdType::RandomHash(), "::1", 3030));
    }

    rt.DumpToLog();

  } catch (std::exception& e) {
    ASLOG_MISC(error, "Error: {}", e.what());
    return -1;
  } catch (...) {
    ASLOG_MISC(error, "Unknown error!");
    return -1;
  }

  return 0;
}
