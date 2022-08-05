//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <nat/blocxxi_nat_api.h>

#include <memory> // for unique_ptr

// #include <common/logging.h>
#include <nat/port_mapper.h>

/// @name Forward declaration of types from miniupnp library.
//@{
struct UPNPUrls;
struct IGDdatas;
//@}

namespace blocxxi::nat {

/*!
 * Implements port mapping following the UPNP method.
 */
class BLOCXXI_NAT_API UpnpPortMapper final : protected PortMapper
//  , asap::logging::Loggable<asap::logging::Id::NAT>
{
public:
  static auto Discover(
      std::chrono::milliseconds timeout) -> std::unique_ptr<PortMapper>;

  /// @name Constructors etc.
  //@{
private:
  /// Default constructor
  UpnpPortMapper();

public:
  /// Not copyable
  UpnpPortMapper(UpnpPortMapper const &) = delete;
  /// Not assignable
  auto operator=(UpnpPortMapper const &) -> UpnpPortMapper & = delete;
  /// Move copyable
  UpnpPortMapper(UpnpPortMapper &&) noexcept;
  /// Move assignable
  auto operator=(UpnpPortMapper &&) noexcept -> UpnpPortMapper &;
  /// Destructor
  ~UpnpPortMapper() override;
  /// Swap
  void Swap(UpnpPortMapper &other) noexcept;
  //@}

  /// @copydoc PortMapper::AddMapping()
  auto AddMapping(Protocol protocol,
      unsigned short external_port, unsigned short internal_port,
      std::string const &name, std::chrono::seconds lease_time) -> std::error_condition override;

  /// @copydoc PortMapper::DeleteMapping()
  auto DeleteMapping(
      Protocol protocol, unsigned short external_port) -> std::error_condition override;

  /// @copydoc PortMapper::ToString()
  /// Always returns "upnp".
  [[nodiscard]] auto ToString() const -> std::string override {
    return "upnp";
  }

private:
  /// miniupnpc data structure holding the UPNP device URLs
  struct UPNPUrls *urls_;
  /// miniupnpc data structure holding data about the UPNP device
  struct IGDdatas *data_;
};

/*!
 * @brief Helper factory to synchronously create an instance of the
 * UpnpPortMapper. UPNP discovery will be done in a blocking mode.
 *
 * @param timeout maximum time in milliseconds to allow the UPNP discovery
 * to run before aborting it and returning with a failure.
 * @return a unique_ptr to a suitable PortMapper if one has been successfully
 * made; nullptr otherwise.
 */
inline auto DiscoverUPNP(std::chrono::milliseconds timeout)
    -> std::unique_ptr<PortMapper> {
  return UpnpPortMapper::Discover(timeout);
}

} // namespace blocxxi::nat
