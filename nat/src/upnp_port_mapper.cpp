//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <cstring>
#ifdef WIN32
#include <winsock2.h>
#endif

#include <miniupnpc.h>
#include <miniwget.h>
#include <upnpcommands.h>

#include <nat/error.h>
#include <nat/upnp_port_mapper.h>

#include <string> // std::string, std::to_string

namespace blocxxi::nat {

UpnpPortMapper::UpnpPortMapper() : urls_(nullptr), data_(nullptr) {
}

UpnpPortMapper::UpnpPortMapper(UpnpPortMapper &&other) noexcept
    : urls_(other.urls_), data_(other.data_) {
  other.urls_ = nullptr;
  other.data_ = nullptr;
}

auto UpnpPortMapper::operator=(UpnpPortMapper &&moved) noexcept
    -> UpnpPortMapper & {
  auto tmp = std::move(moved);
  Swap(tmp);
  return *this;
}

UpnpPortMapper::~UpnpPortMapper() {
  FreeUPNPUrls(urls_);
  free(urls_);
  free(data_);
}

void UpnpPortMapper::Swap(UpnpPortMapper &other) noexcept {
  using std::swap;
  swap(urls_, other.urls_);
  swap(data_, other.data_);
  swap(internal_ip_, other.internal_ip_);
  swap(external_ip_, other.external_ip_);
}

auto UpnpPortMapper::AddMapping(PortMapper::Protocol protocol,
    unsigned external_port, unsigned internal_port, std::string const &name,
    std::chrono::seconds lease_time) -> std::error_condition {
  // ASLOG(debug, "UPNP add mapping ({}/{}) -> ({}, {})", protocol,
  // external_port,
  //     internal_ip_, internal_port);
  if (urls_->controlURL[0] == '\0') {
    // ASLOG(error, "UPNP discovery was not done!");
    return make_error_condition(DISCOVERY_NOT_DONE);
  }

  std::string external_port_str = std::to_string(external_port);
  std::string internal_port_str = std::to_string(internal_port);

  int failure = UPNP_AddPortMapping(urls_->controlURL, data_->first.servicetype,
      external_port_str.c_str(), internal_port_str.c_str(),
      internal_ip_.c_str(), name.c_str(), ProtocolString(protocol), nullptr,
      std::to_string(static_cast<int>(lease_time.count())).c_str());
  if (failure != 0) {
    // TODO: test for error code especially when IGD only supports permanent
    // lease
    // ASLOG(error, "UPNP add mapping ({}, {}, {}) failed, code {}",
    //     external_port_str, external_port_str, internal_ip_, failure);
    return make_error_condition(UPNP_COMMAND_ERROR);
  }

  return {};
}
auto UpnpPortMapper::DeleteMapping(PortMapper::Protocol protocol,
    unsigned external_port) -> std::error_condition {
  // ASLOG(info, "UPNP delete mapping ({}/{})", protocol, external_port);
  if (urls_->controlURL[0] == '\0') {
    // ASLOG(error, "UPNP discovery was not done!");
    return make_error_condition(DISCOVERY_NOT_DONE);
  }

  std::string external_port_str = std::to_string(external_port);
  int failure =
      UPNP_DeletePortMapping(urls_->controlURL, data_->first.servicetype,
          external_port_str.c_str(), ProtocolString(protocol), nullptr);
  return (failure != 0) ? make_error_condition(UPNP_COMMAND_ERROR)
                        : std::error_condition{};
}

auto UpnpPortMapper::Discover(std::chrono::milliseconds timeout)
    -> std::unique_ptr<PortMapper> {
  // ASLOG(debug, "starting discovery for UPNP port mapper");

#ifdef WIN32
  WSADATA wsaData;
  int nResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (nResult != NO_ERROR) {
    // ASLOG(error, "WSAStartup() failed.");
    return nullptr;
  }
#endif

  auto mapper = new UpnpPortMapper();

  mapper->urls_ = static_cast<UPNPUrls *>(malloc(sizeof(struct UPNPUrls)));
  mapper->data_ = static_cast<IGDdatas *>(malloc(sizeof(struct IGDdatas)));
  memset(mapper->urls_, 0, sizeof(struct UPNPUrls));
  memset(mapper->data_, 0, sizeof(struct IGDdatas));

  int error = 0;
  // Needs to be freed before returning
  struct UPNPDev *devlist;
  devlist = upnpDiscover(static_cast<int>(timeout.count()), nullptr, nullptr,
      UPNP_LOCAL_PORT_ANY, 0, 2, &error);
  if (devlist) {
    // Find a valid IGD and get our internal IP
    char lanaddr[16];
    int idg_was_found = UPNP_GetValidIGD(devlist, mapper->urls_, mapper->data_,
        reinterpret_cast<char *>(&lanaddr), 16);
    if (idg_was_found) {
      // ASLOG(debug, "UPNP found valid IGD device (status: {}) desc: {}",
      //     idg_was_found, mapper->urls_->rootdescURL);

      mapper->internal_ip_ = std::string(lanaddr);

      // get external IP adress
      char ip[16];
      if (0 == UPNP_GetExternalIPAddress(mapper->urls_->controlURL,
                   mapper->data_->CIF.servicetype,
                   reinterpret_cast<char *>(&ip))) {
        mapper->external_ip_ = std::string(ip);
        freeUPNPDevlist(devlist);
        return std::unique_ptr<PortMapper>(mapper);
      } else {
        // ASLOG(error, "UPNP failed to obtain external IP");
      }
    } else {
      // ASLOG(error, "UPNP no valid IGD was found");
    }
  }
  // An error happened and we don't have a valid mapper

  freeUPNPDevlist(devlist);
  delete mapper;
  return nullptr;
}

} // namespace blocxxi::nat
