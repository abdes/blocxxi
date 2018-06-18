//
// Created by Abdessattar Sassi on 6/11/18.
//

#include <map>
#include <vector>

//#include <boost/asio.hpp>
#ifdef WIN32
// TODO: windows includes for interfaces enumeration
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#endif  // WIN32

#include <common/logging.h>

#include <nat/nat.h>
#include <nat/upnp_port_mapper.h>

using blocxxi::logging::Logger;
using blocxxi::logging::Registry;

namespace blocxxi {
namespace nat {

namespace {

/**!
 * @bried Check if an IP address (IPV4 or IPV6) is a loopback address.
 *
 * @param address the address to check
 * @param ipv6 indicates if the address is an IPV4 or IPV6 address.
 * @return true if the address is a loopback address; false otherwise.
 */
bool IsLoopback(std::string const &address, bool ipv6 = false) {
  return address == ((ipv6) ? "::1" : "127.0.0.1");
}

/*!
 * @brief Check if the address (IPV4 or IPV6) is a local address or an external
 * one by comparing it to the standard pre-allocated local IP address ranges.
 *
 * @param address the address to check
 * @param ipv6 indicates if the address is an IPV4 or IPV6 address.
 * @return true if the address is a loopback address; false otherwise.
 */
bool IsLocal(std::string address, bool ipv6 = false) {
  if (IsLoopback(address, ipv6)) return true;
  if (ipv6) {
    std::transform(address.begin(), address.end(), address.begin(), ::tolower);
    return address.substr(0, 4) == "fe80";
  } else {
    if (address.substr(0, 3) == "10.") return true;
    if (address.substr(0, 8) == "192.168.") return true;
    if (address.substr(0, 8) == "169.254.") return true;
    auto sub = address.substr(0, 7);
    return sub == "172.16" || sub == "172.17" || sub == "172.18";
  }
}

/*!
 * @brief Use the underlying operating system facilities to discover the system
 * network interfaces and their associated IP addresses, then select the most
 * suitable IPV4 one to be used for the application.
 *
 * Preference is given to an external IP address, if not, to a local IP address
 * but on an interface that has both IPV4 and IPV6 addresses configured, and if
 * not then to a local non-loopback address. If everything fails the loopback
 * address will be returned, unless we fail to even query the system for its
 * network interfaces.
 *
 * @return the most suitable IPV4 address to be used for the application.
 */
std::string FindBestAddress() {
  auto &logger = Registry::GetLogger(blocxxi::logging::Id::NAT);
  // Try to find a suitable interface, preferably with an external IPV4
  // address

  // Enumerate all interfaces and their IP addresses within the IPV4 domain
  struct AddressInfo {
    std::string value_;
    bool is_loopback_;
    bool is_external_;
    bool is_v4_;
  };
  using IfceAddressList = std::vector<AddressInfo>;
  std::map<std::string, IfceAddressList> interfaces;

  // This will hold the best available address or an empty string if we were not
  // able to find one.
  std::string selected_address;

#ifdef WIN32
// TODO: Implement network interface enumeration for Windows
#else   // WIN32
  struct ifaddrs *ifaddr, *ifa;

  BXLOG_TO_LOGGER(logger, debug,
                  "enumerating network interfaces and IP addresses");
  if (getifaddrs(&ifaddr) == -1) {
    BXLOG_TO_LOGGER(
        logger, warn,
        "could not gather any info on available network interfaces (e={})",
        errno);
    return selected_address;
  }

  // Walk through linked list, maintaining head pointer so we can free list
  // later
  for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == nullptr || (ifa->ifa_addr->sa_family != AF_INET &&
                                     ifa->ifa_addr->sa_family != AF_INET6))
      continue;

    char address[45];
    if (ifa->ifa_addr->sa_family == AF_INET) {
      auto res = inet_ntop(ifa->ifa_addr->sa_family,
                           &(((struct sockaddr_in *)(ifa->ifa_addr))->sin_addr),
                           address, 32);
      if (!res) perror("inet_ntop");
    } else {
      auto res = inet_ntop(
          ifa->ifa_addr->sa_family,
          &(((struct sockaddr_in6 *)(ifa->ifa_addr))->sin6_addr), address, 32);
      if (!res) perror("inet_ntop");
    }

    AddressInfo addr_info{
        address, IsLoopback(address, ifa->ifa_addr->sa_family == AF_INET6),
        !IsLocal(address, ifa->ifa_addr->sa_family == AF_INET6),
        ifa->ifa_addr->sa_family == AF_INET};

    BXLOG_TO_LOGGER(logger, debug, "{}: {} {} {} {}", ifa->ifa_name,
                    addr_info.is_v4_ ? "v4" : "v6",
                    addr_info.is_external_ ? "ext" : "loc", addr_info.value_,
                    addr_info.is_loopback_ ? "(loopback)" : "");

    interfaces[ifa->ifa_name].push_back(addr_info);
  }
  freeifaddrs(ifaddr);
#endif  // WIN32

  // We only select IPV4 address
  for (auto it : interfaces) {
    auto has_v4 = false;
    auto has_v6 = false;
    auto definitely_found = false;
    for (auto &ainfo : it.second) {
      // We got an external IP address on this interface -> best candidate
      if (ainfo.is_external_ && ainfo.is_v4_) {
        selected_address = ainfo.value_;
        definitely_found = true;
        break;
      }
      // Not external
      else {
        // If it's the loopback interface and we still have not seen any
        // non-loopback address yet, just keep it as this may end up as our
        // only option
        if (ainfo.is_loopback_) {
          if (selected_address.empty()) selected_address = ainfo.value_;
        } else {
          // We need IPv4 as many gateways are still configured for IPv4 only.
          // We also prefer interfaces that have both the v4 and v6 provisioned,
          // as this indicates the main interface of the system.
          if (ainfo.is_v4_) {
            has_v4 = true;
            selected_address = ainfo.value_;
          } else {
            has_v6 = true;
          }
          if (has_v4 && has_v6) {
            definitely_found = true;
            break;
          }
        }
      }
    }
    // If we have already found a suitable address, no need to go any further
    if (definitely_found) break;
  }

  BXLOG_TO_LOGGER(logger, info, "selected address: {}",
                  selected_address.empty() ? "none" : selected_address);

  return selected_address;
}

}  // namespace

std::unique_ptr<PortMapper> GetPortMapper(std::string const &spec) {
  auto &logger = Registry::GetLogger(blocxxi::logging::Id::NAT);

  BXLOG_TO_LOGGER(logger, debug, "NAT port mapper spec: [{}]", spec);

  std::vector<std::string> parts;
  auto token_start = 0UL;
  auto token_end = spec.find(':');
  while (token_end != std::string::npos) {
    parts.emplace_back(spec.substr(token_start, token_end - token_start));
    token_start = token_end + 1;
    token_end = spec.find(':', token_start);
  }
  parts.emplace_back(spec.substr(token_start));

  PortMapper *mapper = nullptr;
  auto mechanism = parts[0];
  if (mechanism == "extip") {
    if (parts.size() == 3) {
      mapper = new NoPortMapper(parts[1], parts[2]);
    } else if (parts.size() == 2) {
      mapper = new NoPortMapper(parts[1], parts[1]);
    } else {
      BXLOG_TO_LOGGER(logger, debug,
                      "Missing explicit external IP in 'extip' nat spec");
    }
    BXLOG_TO_LOGGER(logger, debug, "Using mapper {}", mapper->ToString());
    return std::unique_ptr<PortMapper>(mapper);

  } else if (mechanism == "upnp") {
    return DiscoverUPNP(std::chrono::milliseconds(2000));

  } else if (mechanism == "pmp") {
    // TODO: Not implemented yet (pmp)
    BXLOG_TO_LOGGER(logger, error, "PMP port mapper is not implemented yet");
    return nullptr;

  } else {
    // Try to auto discover the environment

    auto address = FindBestAddress();
    if (address.empty()) {
      return nullptr;
    } else if (IsLocal(address)) {
      // Try UPNP
      auto upnp = DiscoverUPNP(std::chrono::milliseconds(2000));
      if (upnp) return upnp;

      // TODO: Try PMP

      BXLOG_TO_LOGGER(logger, warn, "Could not discover external IP address.");
      BXLOG_TO_LOGGER(logger, info,
                      "Consider using nat spec 'extip' to manually specify it");
    }
    mapper = new NoPortMapper(address, address);
    return std::unique_ptr<PortMapper>(mapper);
  }
}

}  // namespace nat
}  // namespace blocxxi
