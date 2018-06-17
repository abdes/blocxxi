//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#ifndef BLOCXXI_NAT_UPNP_PORT_MAPPER_H_
#define BLOCXXI_NAT_UPNP_PORT_MAPPER_H_

#include <memory>  // for unique_ptr

#include <common/logging.h>
#include <nat/port_mapper.h>

/// @name Forward declaration of types from miniupnp library.
//@{
struct UPNPUrls;
struct IGDdatas;
//@}

namespace blocxxi {
namespace nat {

/*!
 * Implements port mapping following the UPNP method.
 */
class UpnpPortMapper final : protected PortMapper,
                             blocxxi::logging::Loggable<logging::Id::NAT> {
 public:
  static std::unique_ptr<PortMapper> Discover(
      std::chrono::milliseconds timeout);

  /// @name Constructors etc.
  //@{
 private:
  /// Default constructor
  UpnpPortMapper();

 public:
  /// Not copyable
  UpnpPortMapper(UpnpPortMapper const &) = delete;
  /// Not assignable
  UpnpPortMapper &operator=(UpnpPortMapper const &) = delete;
  /// Move copyable
  UpnpPortMapper(UpnpPortMapper &&) noexcept;
  /// Move assignable
  UpnpPortMapper &operator=(UpnpPortMapper &&) noexcept;
  /// Destructor
  ~UpnpPortMapper() override;
  /// Swap
  void Swap(UpnpPortMapper &other) noexcept;
  //@}

  /// @copydoc PortMapper::AddMapping()
  std::error_condition AddMapping(Protocol protocol,
                                  unsigned short external_port,
                                  unsigned short internal_port,
                                  std::string const &name,
                                  std::chrono::seconds lease_time) override;

  /// @copydoc PortMapper::DeleteMapping()
  std::error_condition DeleteMapping(Protocol protocol,
                                     unsigned short external_port) override;

  /// @copydoc PortMapper::ToString()
  /// Always returns "upnp".
  std::string ToString() const override { return "upnp"; }

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
inline std::unique_ptr<PortMapper> DiscoverUPNP(
    std::chrono::milliseconds timeout) {
  return UpnpPortMapper::Discover(timeout);
}

}  // namespace nat
}  // namespace blocxxi

#endif  // BLOCXXI_NAT_UPNP_PORT_MAPPER_H_
