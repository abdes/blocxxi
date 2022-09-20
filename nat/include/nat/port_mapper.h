//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

/*!
 * \file
 *
 * \brief Abstract base class for NAT port mapping implementations (e.g. UPNP,
 * PMP, etc.).
 */

#pragma once

#include <chrono>
#include <system_error>
#include <utility>

#include <common/compilers.h>

// Disable compiler warnings produced by fmtlib.
ASAP_DIAGNOSTIC_PUSH
#if defined(ASAP_CLANG_VERSION)
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif
#if defined(ASAP_GNUC_VERSION)
ASAP_PRAGMA(GCC diagnostic ignored "-Wswitch-default")
ASAP_PRAGMA(GCC diagnostic ignored "-Wswitch-enum")
#endif
#include <fmt/format.h>
ASAP_DIAGNOSTIC_POP

#include <magic_enum.hpp>

#include <nat/blocxxi_nat_export.h>

namespace blocxxi::nat {

/// Abstract interface for NAT port mapping methods such as UPNP, PMP, etc...
class BLOCXXI_NAT_API PortMapper {
public:
  /// The possible values for the PortMappingProtocol.
  enum class Protocol { TCP, UDP };

  static auto ProtocolString(Protocol protocol) noexcept -> std::string_view;

  /// Default lease time in seconds.
  static constexpr std::chrono::seconds c_default_lease_time{3600};

  struct Mapping {
    /// The protocol of the port mapping. Possible values are TCP or UDP.
    Protocol protocol{Protocol::UDP};
    /// The external port that the NAT gateway would “listen” on for connection
    /// requests to a corresponding InternalPort on an InternalClient. Inbound
    /// packets to this external port on the WAN interface of the gateway SHOULD
    /// be forwarded to InternalClient on the InternalPort on which the message
    /// was received.
    unsigned external_port{0};
    /// The port on InternalClient that the gateway SHOULD forward connection
    /// requests to. A value of 0 is not allowed. NAT implementations that do
    /// not permit different values for ExternalPort and InternalPort will
    /// return an error.
    unsigned internal_port{0};
    /// A textual description of the port mapping for diagnostics etc.
    std::string const &name;
  };

  /// Default.
  PortMapper() = default;

  /// Construct from known internal and external IP addresses.
  PortMapper(std::string externla_ip, std::string internal_ip)
      : external_ip_(std::move(externla_ip)),
        internal_ip_(std::move(internal_ip)) {
  }

  /// Default.
  virtual ~PortMapper();

  PortMapper(const PortMapper &) = default;
  PortMapper(PortMapper &&) = default;
  auto operator=(const PortMapper &) -> PortMapper & = default;
  auto operator=(PortMapper &&) -> PortMapper & = default;

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
   * @param [in] mapping the port mapping to add.
   * @param [in] lease_time determines the lifetime in seconds of a port-mapping
   * lease. Non-zero values indicate the duration after which a port mapping
   * will be removed, unless a control point refreshes the mapping.
   * @return An error status indicating success or failure.
   */
  virtual auto AddMapping(Mapping mapping, std::chrono::seconds lease_time)
      -> std::error_condition = 0;

  auto AddMapping(Mapping mapping) -> std::error_condition {
    return AddMapping(mapping, c_default_lease_time);
  }

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
  [[nodiscard]] virtual auto MapperType() const -> std::string = 0;

protected:
  std::string external_ip_;
  std::string internal_ip_;
};

} // namespace blocxxi::nat

template <>
struct fmt::formatter<blocxxi::nat::PortMapper::Protocol>
    : formatter<std::string_view> {
  // parse is inherited from formatter<string_view>.
  template <typename FormatContext>
  auto format(const blocxxi::nat::PortMapper::Protocol &protocol,
      FormatContext &ctx) const {
    string_view name = magic_enum::enum_name(protocol);
    return formatter<string_view>::format(name, ctx);
  }
};

template <>
struct fmt::formatter<blocxxi::nat::PortMapper> : formatter<std::string_view> {
  // parse is inherited from formatter<string_view>.
  template <typename FormatContext>
  auto format(
      const blocxxi::nat::PortMapper &mapper, FormatContext &ctx) const {
    std::string text = fmt::format("{}:{}:{}", mapper.MapperType(),
        mapper.ExternalIP(), mapper.InternalIP());
    return formatter<string_view>::format(text, ctx);
  }
};
