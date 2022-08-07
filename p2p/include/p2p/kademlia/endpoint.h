//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <p2p/blocxxi_p2p_api.h>

#include <iostream>

#include <boost/asio.hpp>
#include <utility>

namespace blocxxi::p2p::kademlia {

class BLOCXXI_P2P_API IpEndpoint final {
public:
  using AddressType = boost::asio::ip::address;

  IpEndpoint() = default;
  IpEndpoint(AddressType address, unsigned short port)
      : address_(std::move(address)), port_(port) {
  }

  IpEndpoint(const std::string &address, unsigned short port)
      : address_(boost::asio::ip::address::from_string(address)), port_(port) {
  }

  IpEndpoint(IpEndpoint const &) = default;
  auto operator=(IpEndpoint const &) -> IpEndpoint & = default;

  IpEndpoint(IpEndpoint &&) = default;
  auto operator=(IpEndpoint &&) -> IpEndpoint & = default;

  ~IpEndpoint() = default;

  [[nodiscard]] auto Address() const -> AddressType const & {
    return address_;
  }
  void Address(const AddressType &address) {
    address_ = address;
  }
  [[nodiscard]] auto Port() const -> std::uint16_t {
    return port_;
  }
  void Port(std::uint16_t port) {
    port_ = port;
  }

  [[nodiscard]] auto ToString() const -> std::string {
    return address_.to_string() + ":" + std::to_string(port_);
  }

  boost::asio::ip::address address_;
  std::uint16_t port_{0};
};

inline auto operator==(IpEndpoint const &a, IpEndpoint const &b) -> bool {
  return a.address_ == b.address_ && a.port_ == b.port_;
}

inline auto operator!=(IpEndpoint const &a, IpEndpoint const &b) -> bool {
  return !(a == b);
}

inline auto operator<<(std::ostream &os, IpEndpoint const &ep)
    -> std::ostream & {
  os << ep.Address().to_string() << ":" << ep.Port();
  return os;
}

} // namespace blocxxi::p2p::kademlia
