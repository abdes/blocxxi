//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#ifndef BLOCXXI_P2P_KADEMLIA_ENDPOINT_H_
#define BLOCXXI_P2P_KADEMLIA_ENDPOINT_H_

#include <iostream>

#include <boost/asio.hpp>

namespace blocxxi {
namespace p2p {
namespace kademlia {

class IpEndpoint final {
 public:
  using AddressType = boost::asio::ip::address;

 public:
  IpEndpoint() = default;
  IpEndpoint(const AddressType &address, unsigned short port)
      : address_(address), port_(port) {}

  IpEndpoint(const std::string &address, unsigned short port)
      : address_(boost::asio::ip::address::from_string(address)), port_(port) {}

  IpEndpoint(IpEndpoint const &) = default;
  IpEndpoint &operator=(IpEndpoint const &) = default;

  IpEndpoint(IpEndpoint &&) = default;
  IpEndpoint &operator=(IpEndpoint &&) = default;

  ~IpEndpoint() = default;

  AddressType const &Address() const { return address_; }
  void Address(const AddressType &address) { address_ = address; }
  std::uint16_t Port() const { return port_; }
  void Port(std::uint16_t port) { port_ = port; }

  std::string ToString() const {
    return address_.to_string() + ":" + std::to_string(port_);
  }

  boost::asio::ip::address address_;
  std::uint16_t port_{0};
};

inline bool operator==(IpEndpoint const &a, IpEndpoint const &b) {
  return a.address_ == b.address_ && a.port_ == b.port_;
}

inline bool operator!=(IpEndpoint const &a, IpEndpoint const &b) {
  return !(a == b);
}

inline std::ostream &operator<<(std::ostream &os, IpEndpoint const &ep) {
  os << ep.Address().to_string() << ":" << ep.Port();
  return os;
}

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi

#endif  // BLOCXXI_P2P_KADEMLIA_ENDPOINT_H_
