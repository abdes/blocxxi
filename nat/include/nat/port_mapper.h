//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <nat/blocxxi_nat_api.h>

#include <chrono> // for duration
#include <iosfwd> // for implementation of operator<<
#include <iostream>
#include <system_error> // for std:;error_condition
#include <utility>

namespace blocxxi::nat {

/// Abstract interface for NAT port mapping methods such as UPNP, PMP, etc...
class BLOCXXI_NAT_API PortMapper {
public:
  /// The possible values for the PortMappingProtocol.
  enum class Protocol { TCP, UDP };

  static auto ProtocolString(Protocol protocol) -> char const * {
    switch (protocol) {
    case Protocol::TCP:
      return "TCP";
    case Protocol::UDP:
      return "UDP";
    }
    // Only needed for compilers that complain about not all control paths
    // return a value.
    return "UDP";
  }

  /// Default.
  PortMapper() = default;

  /// Construct from known internal and external IP addresses.
  PortMapper(std::string externla_ip, std::string internal_ip)
      : external_ip_(std::move(externla_ip)),
        internal_ip_(std::move(internal_ip)) {
  }

  /// Default.
  virtual ~PortMapper() = default;

  /*!
   * @brief Add a mapping between a port on the local machine to a port that
   * can be connected to from the internet.
   *
   * This action creates a new port mapping or overwrites an existing mapping
   * with the same internal client. If the ExternalPort and PortMappingProtocol
   * pair is already mapped to another internal client, an error is returned.
   *
   * It is recommended to use the same values for InternalPort and ExternalPort
   * as not all UPNP IGDs support port mapping with different values.
   *
   * PortMappingLeaseDuration value can be between 1 second and 604800 seconds,
   * however value “0” can exist for port mappings made out of band.
   * Infinite mappings cannot be made in WANIPConnection:2 and value “0” WILL
   * be interpreted as 604800 seconds.
   *
   * Some UPNP IGDs only support permanent leases (value 0) and will return
   * OnlyPermanentLeasesSupported error. In such case, the implementation must
   * fallback to request a permanent lease.
   *
   * It is RECOMMENDED that a lease time of 3600 seconds would be used as a
   * default value.
   *
   * @param [in] protocol represents the protocol of the port mapping. Possible
   * values are TCP or UDP.
   * @param [in] external_port represents the external port that the NAT gateway
   * would “listen” on for connection requests to a corresponding InternalPort
   * on an InternalClient. Inbound packets to this external port on the WAN
   * interface of the gateway SHOULD be forwarded to InternalClient on the
   * InternalPort on which the message was received.
   * @param [in] internal_port represents the port on InternalClient that the
   * gateway SHOULD forward connection requests to. A value of 0 is not allowed.
   * NAT implementations that do not permit different values for ExternalPort
   * and InternalPort will return an error.
   * @param [in] name a string representation of a port mapping.
   * @param [in] lease_time determines the lifetime in seconds of a port-mapping
   * lease. Non-zero values indicate the duration after which a port mapping
   * will be removed, unless a control point refreshes the mapping.
   * @return An error status indicating success or failure.
   */
  virtual auto AddMapping(Protocol protocol, unsigned external_port,
      unsigned internal_port, std::string const &name,
      std::chrono::seconds lease_time) -> std::error_condition = 0;

  /*!
   * @brief Delete a previously instantiated port mapping.
   *
   * @param [in] protocol represents the protocol of the port mapping. Possible
   * values are TCP or UDP.
   * @param [in] external_port the external port value of a port mapping.
   * @return An error status indicating success or failure.
   */
  virtual auto DeleteMapping(Protocol protocol, unsigned external_port)
      -> std::error_condition = 0;

  /// This method should return the external (Internet-facing)
  /// address of the gateway device.
  [[nodiscard]] virtual auto ExternalIP() const -> std::string const & {
    return external_ip_;
  }

  /// This method should return the internal (lan) client address.
  [[nodiscard]] virtual auto InternalIP() const -> std::string const & {
    return internal_ip_;
  }

  /// Should return the name of the concrete port mapping method.
  /// This is used for logging.
  [[nodiscard]] virtual auto ToString() const -> std::string = 0;

protected:
  std::string external_ip_;
  std::string internal_ip_;
};

/// Send the string representation of the protocol constant to the
/// output stream.
inline auto operator<<(std::ostream &out, PortMapper::Protocol protocol)
    -> std::ostream & {
  out << PortMapper::ProtocolString(protocol);
  return out;
}

} // namespace blocxxi::nat
