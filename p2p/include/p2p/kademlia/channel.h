//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#ifndef BLOCXXI_P2P_KADEMLIA_CHANNEL_H_
#define BLOCXXI_P2P_KADEMLIA_CHANNEL_H_

#include <memory>

#include <boost/asio.hpp>

#include <common/assert.h>
#include <common/logging.h>

#include <p2p/kademlia/buffer.h>
#include <p2p/kademlia/endpoint.h>

#include <p2p/kademlia/detail/error_impl.h>

namespace blocxxi {
namespace p2p {
namespace kademlia {

/*!
 * @brief A Channel encapsulates an underlying socket and uses it to provide an
 * asynchronous interface for sending and receiving messages.
 *
 * @tparam TUnderlyingSocket the underlying socket type.
 */
template <typename TUnderlyingSocket>
class Channel final : asap::logging::Loggable<asap::logging::Id::P2P_KADEMLIA> {
 public:
  /// @name Type shortcuts
  ///@{
  using PointerType = std::unique_ptr<Channel>;
  using EndpointType = IpEndpoint;
  ///@}

  /// The protocol does not send messages more than this size to keep every
  /// message in a single UDP datagram.
  static constexpr std::size_t SAFE_PAYLOAD_SIZE = 1452;

  /// @name Constructors and factories
  ///@{
  /*!
   * @brief Create a Channel object bound to the given endpoint.
   *
   * At construction, the underlying socket is created based on the protocol
   * corresponding to the given endpoint (IPv4 or IPv6) and the socket is bound.
   *
   * @param io_context io_context to be used for the socket operations.
   * @param ep the endpoint to which the underlying socket must be bound.
   */
  Channel(boost::asio::io_context &io_context, EndpointType const &ep)
      : reception_buffer_(SAFE_PAYLOAD_SIZE),
        current_message_sender_(),
        socket_(CreateUnderlyingSocket(io_context, ep)) {
    ASLOG(debug, "Creating channel {} DONE", ep);
  }

  /*!
   * @brief Destroy the channel object, closing the underlying socket. Errors
   * are ignored.
   */
  ~Channel() {
    ASLOG(debug, "Destroy channel {}", LocalEndpoint());
    try {
      socket_.close();
    } catch (std::exception const &ex) {
      // Ignore all errors. The underlying descriptor will be closed anyway.
      ASLOG(debug, "an error reported on socket closure: {}", ex.what());
    }
  }

  /// Not copyable
  Channel(Channel const &) = delete;
  /// Not copyable
  Channel &operator=(Channel const &) = delete;

  /// Default
  Channel(Channel &&) = default;
  /// Default
  Channel &operator=(Channel &&) = default;

  /*!
   * @brief Create a channel for the IPv4 protocol.
   *
   * The given host and service are resolved per the IPv4 protocol before the
   * under channel and its underlying socket are created.
   *
   * @param [in] io_context for async operations, must be run by the caller.
   * @param [in] host host name or IPv4 address. Whatever is provided will be
   * resolved as and Ipv4 address. If the resolution fails to produce an IPv4
   * address (or more), a system_error exception is thrown.
   * @param [in] service service name or port number. Whatever is provided will
   * be resolved to a port number.
   * @return A chanel object configured with an underlying socket using an IPv4
   * endpoint.
   * @throw std::system_error if the resolution does not produce a valid IPv4
   * endpoint.
   */
  static PointerType ipv4(boost::asio::io_context &io_context,
                          std::string const &host, std::string const &service) {
    try {
      auto endpoints = ResolveEndpoint(io_context, host, service);

      for (auto const &ep : endpoints) {
        if (ep.address_.is_v4())
          return std::make_unique<Channel>(io_context, ep);
      }
    } catch (std::exception const &) {
      throw std::system_error{detail::make_error_code(INVALID_IPV4_ADDRESS)};
    }

    ASLOG(error, "({} / {}) did not resolve to a valid IPv4 endpoint", host,
          service);
    throw std::system_error{detail::make_error_code(INVALID_IPV4_ADDRESS)};
  }

  /*!
   * @brief Create a channel for the IPv6 protocol.
   *
   * The given host and service are resolved per the IPv6 protocol before the
   * under channel and its underlying socket are created.
   *
   * @param [in] io_context for async operations, must be run by the caller.
   * @param [in] host host name or IPv6 address. Whatever is provided will be
   * resolved as and Ipv4 address. If the resolution fails to produce an IPv6
   * address (or more), a system_error exception is thrown.
   * @param [in] service service name or port number. Whatever is provided will
   * be resolved to a port number.
   * @return A chanel object configured with an underlying socket using an IPv6
   * endpoint.
   * @throw std::system_error if the resolution does not produce a valid IPv6
   * endpoint.
   */
  static PointerType ipv6(boost::asio::io_context &io_context,
                          std::string const &host, std::string const &service) {
    try {
      auto endpoints = ResolveEndpoint(io_context, host, service);

      for (auto const &ep : endpoints) {
        if (ep.address_.is_v6())
          return std::make_unique<Channel>(io_context, ep);
      }
    } catch (std::exception const &) {
      throw std::system_error{detail::make_error_code(INVALID_IPV4_ADDRESS)};
    }

    ASLOG(error, "({} / {}) did not resolve to a valid IPv6 endpoint", host,
          service);
    throw std::system_error{detail::make_error_code(INVALID_IPV6_ADDRESS)};
  }
  ///@}

  /*!
   * @brief Asynchronously receive a message from the underlying socket and call
   * the completion callback.
   *
   * The protocol is designed in such a way that messages sent/received fit into
   * the maximum size of the underlying socket. Hence, we can assume that every
   * successful asynchronous receive on the channel returns enough data for a
   * message to be deserialized.
   *
   * @tparam TReceiveCallback the completion callback type.
   * @param [in] callback the completion callback. The failure parameter of the
   * callback is a std::system_error indicating success or an error. The
   * callback is invoked with the following arguments:
   *   - std::system_error indicating success or failure
   *   - EndpointType object containing the sender endpoint
   *   - iterator at the start of the received buffer
   *   - iterator at the end (past the last valid element) of the received
   *     buffer
   *
   *  @see AsyncSend()
   */
  template <typename TReceiveCallback>
  void AsyncReceive(TReceiveCallback const &callback) {
    auto on_completion = [this, callback](
        boost::system::error_code const &failure, std::size_t bytes_received) {
#ifdef _MSC_VER
      // On Windows, an UDP socket may return connection_reset
      // to inform application that a previous send by this socket
      // has generated an ICMP port unreachable.
      // https://msdn.microsoft.com/en-us/library/ms740120.aspx
      // Ignore it and schedule another read.
      if (failure == boost::system::errc::connection_reset)
        return AsyncReceive(callback);
#endif
      callback(detail::BoostToStdError(failure),
               ConvertEndpoint(current_message_sender_),
               BufferReader(reception_buffer_.data(), bytes_received));
    };

    ASAP_ASSERT(reception_buffer_.size() == SAFE_PAYLOAD_SIZE);
    socket_.async_receive_from(boost::asio::buffer(reception_buffer_),
                               current_message_sender_, on_completion);
  }

  /*!
   * @brief Asynchronously send data over the underlying socket.
   *
   * The size of the data buffer should not exceed the maximum size that fits
   * into a single IO operation over the underlying socket. For UDP, given the
   * ethernet MTU is 1500, the IPv6 header size is 40 bytes, IPv4 header size is
   * 20 bytes and the UDP header is 8 bytes, we are using a maximum safe payload
   * size of 1500-40-8 = 1452bytes.
   *
   * @tparam TSendCallback the completion callback type.
   * @param [in] message the data buffer to be sent (result of message
   * serialization). The passed object must survive until the completion
   * callback is invoked.
   * @param [in] to the destination endpoint.
   * @param [in] callback the completion callback. The callback is invoked with
   * the following arguments:
   *   - std::system_error indicating success or failure
   *
   * @see SAFE_PAYLOAD_SIZE
   * @see AsyncReceive()
   */
  template <typename TSendCallback>
  void AsyncSend(Buffer const &message, IpEndpoint const &to,
                 TSendCallback const &callback) {
    if (message.size() > SAFE_PAYLOAD_SIZE)
      callback(make_error_code(std::errc::value_too_large));
    else {
      // Copy the buffer as it has to live past the end of this call.
      auto message_copy = std::make_shared<Buffer>(message);
      auto on_completion = [callback, message_copy](
          boost::system::error_code const &failure,
          std::size_t /* bytes_sent */) {
        if (failure) ASLOG(error, "{}", failure.message());
        callback(detail::BoostToStdError(failure));
      };

      socket_.async_send_to(boost::asio::buffer(*message_copy),
                            ConvertEndpoint(to), on_completion);
    }
  }

  /*!
   * @brief Retrieve the local endpoint of this channel.
   *
   * @return the local endpoint of the channel.
   */
  EndpointType LocalEndpoint() const {
    return EndpointType(socket_.local_endpoint().address(),
                        socket_.local_endpoint().port());
  }

  /*!
   * @brief Resolve a host name and a service name into an endpoint suitable for
   * the protocol used by the channel's underlying socket.
   *
   * This method is synchronous and therefore blocks until the resolution is
   * complete.
   *
   * @param [in] io_context io_context used to submit the resolution operation.
   * @param [in] host host name to be resolved (may also contain an IP address).
   * @param [in] service service name to be resolved (may also contain a port
   * number).
   * @return a list of resolved endpoints corresponding to the given host and
   * service names.
   *
   * @throw std::system_error if the resolution fails.
   */
  static std::vector<IpEndpoint> ResolveEndpoint(
      boost::asio::io_context &io_context, std::string const &host,
      std::string const &service) {
    using protocol_type = typename UnderlyingSocketType::protocol_type;

    typename protocol_type::resolver r{io_context};
    // One raw endpoint (e.g. localhost) can be resolved to
    // multiple endpoints (e.g. IPv4 / IPv6 address).
    std::vector<EndpointType> endpoints;

    try {
      auto results_iter = r.resolve(host, service);
      for (decltype(results_iter) end; results_iter != end; ++results_iter) {
        // Convert from underlying_endpoint_type to endpoint_type.
        endpoints.push_back(ConvertEndpoint(*results_iter));
      }
      return endpoints;
    } catch (boost::system::system_error const &failure) {
      ASLOG(error, "{}", failure.what());
      throw(std::system_error(detail::BoostToStdError(failure.code())));
    }
  }

 private:
  /// @name Private type shortcuts
  ///@{
  /// The underlying socket type.
  using UnderlyingSocketType = TUnderlyingSocket;
  /// The underlying endpoint type suitable for the underlying socket type.
  using UnderlyingEndpointType = typename UnderlyingSocketType::endpoint_type;
  ///@}

  /*!
   * @brief Create the underlying socket.
   *
   * Depending on whether the used endpoint has an IPv4 or and IPv6 address
   * inside, the underlying socket is created for the appropriate protocol. The
   * socket is setup to allow for bind multiple times on the same address
   * (SO_REUSE_ADDR) and bound to the endpoint.
   *
   * @param [in] io_context io_context used for the socket operations. Must be
   * run by the caller.
   * @param [in] ep endpoint to which the underlying socket will be bound.
   * @return a socket object properly setup and bound to the endpoint.
   *
   * @throw std::system_error if the resolution fails.
   */
  static UnderlyingSocketType CreateUnderlyingSocket(
      boost::asio::io_context &io_context, EndpointType const &ep) {
    UnderlyingEndpointType const uep = ConvertEndpoint(ep);

    try {
      UnderlyingSocketType new_socket{io_context, uep.protocol()};

      if (uep.address().is_v6()) {
        new_socket.set_option(boost::asio::ip::v6_only{true});
      }
      boost::asio::socket_base::reuse_address option(true);
      new_socket.set_option(option);
      new_socket.bind(uep);

      return std::move(new_socket);
    } catch (boost::system::system_error const &failure) {
      ASLOG(error, "{}", failure.what());
      throw(std::system_error(detail::BoostToStdError(failure.code())));
    }
  }

  /*!
   * @brief Convert an underlying endpoint into the equivalent generic endpoint.
   *
   * @param [in] uep underlying endpoint.
   * @return the equivalent generic endpoint.
   */
  static EndpointType ConvertEndpoint(UnderlyingEndpointType const &uep) {
    return EndpointType{uep.address(), uep.port()};
  }

  /*!
   * Convert a generic endpoint into the equivalent underlying endpoint.
   * @param [in] ep generic endpoint.
   * @return the equivalent underlying endpoint.
   */
  static UnderlyingEndpointType ConvertEndpoint(EndpointType const &ep) {
    return UnderlyingEndpointType{ep.Address(), ep.Port()};
  }

  /// The pre-allocated buffer used for receiving messages from the socket.
  /// @see SAFE_PAYLOAD_SIZE
  Buffer reception_buffer_;
  /// The underlying endpoint corresponding to the sender of the currently
  /// received message.
  UnderlyingEndpointType current_message_sender_;
  /// The underlying socket.
  UnderlyingSocketType socket_;
};

/// Template instantiation for an async UDP socket.
using AsyncUdpChannel = Channel<boost::asio::ip::udp::socket>;

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi

#endif  // BLOCXXI_P2P_KADEMLIA_CHANNEL_H_
