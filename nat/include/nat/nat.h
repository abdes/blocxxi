//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <nat/blocxxi_nat_api.h>

#include <memory> // for unique_ptr
#include <vector> // for string splitting

#include <nat/port_mapper.h>

namespace blocxxi::nat {

/*!
 * @brief Implements the PortMapper interface for situations where no port
 * forwarding is needed.
 */
class BLOCXXI_NAT_API NoPortMapper : public PortMapper {
public:
  /// @name Constructors etc.
  //@{
  /*!
   * @brief Create a NoPortMapper instance and initialize it with the provided
   * external and internal addresses.
   *
   * This port mapper can be used for two scenarios:
   *   - when we already have an external IP address (internal_ip is the same
   *   than external_ip
   *   - when port forwarding has already been statically setup at the gateway
   *   (logically internal_ip and external_ip would be different)
   *
   * @param [in] external_ip the external (internet facing) IP address
   * @param [in] internal_ip the internal (LAN facing) IP address
   */
  NoPortMapper(std::string const &external_ip, std::string const &internal_ip)
      : PortMapper(external_ip, internal_ip) {
  }
  /// Not copyable
  NoPortMapper(NoPortMapper const &) = delete;
  /// Not assignable
  auto operator=(NoPortMapper const &) -> NoPortMapper & = delete;
  /// Move copyable
  NoPortMapper(NoPortMapper &&) = default;
  /// Move assignable
  auto operator=(NoPortMapper &&) -> NoPortMapper & = default;
  /// Default
  ~NoPortMapper() override = default;
  //@}

  /// Does nothing.
  auto AddMapping(Protocol /*protocol*/, unsigned short /*external_port*/,
      unsigned short /*internal_port*/, std::string const & /*name*/,
      std::chrono::seconds /*lease_time*/) -> std::error_condition override {
    return {};
  }

  /// Does nothing.
  auto DeleteMapping(Protocol /*protocol*/, unsigned short /*external_port*/)
      -> std::error_condition override {
    return {};
  }

  /// @copydoc PortMapper::ToString()
  [[nodiscard]] auto ToString() const -> std::string override {
    return std::string("NoPortMapper(ext=")
        .append(ExternalIP())
        .append(", int=")
        .append(InternalIP())
        .append(")");
  }
};

/*!
 * @brief Factory that creates and returns the suitable PortMapper instance to
 * meet the nat spec passed as an argument.
 *
 * The currently supported formats for the spec are:
 *   - "upnp" : attempt to dicover the IGD using UPNP and dynamically setup the
 *   port mapping and obtain the internal and external addresses through UPNP.
 *   - "extip:1.2.3.4" : Use the address provided as both the internal and
 *   external address. No port mapping is required.
 *   - "extip:1.2.3.4:192.168.1.42" : Use the first address as the external
 *   address and the second one as the internal address. Assume port forwarding
 *   has already been separately done.
 *   - "" : attempt to discover the environment automatically. If UPNP is
 *   available it will be used, otherwise some best effort heuristics will be
 *   used to detect the available interfaces and select the most suitable
 *   address to use. Preference goes for an external IP address, if not, it goes
 *   for the most likely internal IP address to be associated with the main
 *   interface.
 *
 * @param [in] spec a string specifying the requested nat environment as per the
 * format described in the details above.
 * @return a unique_ptr to a suitable PortMapper if one has been successfully
 * made; nullptr otherwise.
 */
BLOCXXI_NAT_API auto GetPortMapper(std::string const &spec)
    -> std::unique_ptr<PortMapper>;

} // namespace blocxxi::nat
