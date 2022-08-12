//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <iostream>
#include <utility>

#include <p2p/kademlia/boost_asio.h>

#include <p2p/blocxxi_p2p_api.h>

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
    return address_.to_string() + ":" +
           std::to_string(static_cast<unsigned int>(port_));
  }

  boost::asio::ip::address address_;
  std::uint16_t port_{0};
};

inline auto operator==(IpEndpoint const &lhs, IpEndpoint const &rhs) -> bool {
  return lhs.address_ == rhs.address_ && lhs.port_ == rhs.port_;
}

inline auto operator!=(IpEndpoint const &lhs, IpEndpoint const &rhs) -> bool {
  return !(lhs == rhs);
}

inline auto operator<<(std::ostream &out, IpEndpoint const &endpoint)
    -> std::ostream & {
  out << endpoint.Address().to_string() << ":" << endpoint.Port();
  return out;
}

} // namespace blocxxi::p2p::kademlia
