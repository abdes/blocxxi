//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>

#include <boost/asio/io_context.hpp>
#include <utility>

// #include <common/assert.h>
#include <logging/logging.h>
#include <p2p/kademlia/channel.h>
#include <p2p/kademlia/message_serializer.h>
#include <p2p/kademlia/response_dispatcher.h>

namespace blocxxi::p2p::kademlia {

/*!
 * @brief The Network class encapsulates the communication channels (IPv4 and
 * IPv6) and provides message based communication services to the Engine with
 * request/response correlation.
 *
 * Except for name resolution, all send/receive operations are asynchronous.
 *
 * @tparam TChannel underlying communication channel type.
 * @tparam TMessageSerializer message serialization type.
 */
template <typename TChannel, typename TMessageSerializer>
class Network : asap::logging::Loggable<Network<TChannel, TMessageSerializer>> {
public:
  /// The logger id used for logging within this class.
  static constexpr const char *LOGGER_NAME = "p2p-kademlia";

  // We need to import the internal logger retrieval method symbol in this
  // context to avoid g++ complaining about the method not being declared before
  // being used. THis is due to the fact that the current class is a template
  // class and that method does not take any template argument that will enable
  // the compiler to resolve it unambiguously.
  using asap::logging::Loggable<Network<TChannel,
      TMessageSerializer>>::internal_log_do_not_use_read_comment;

  /// @name Type shortcuts
  //@{
  using EndpointType = IpEndpoint;
  using ChannelType = TChannel;
  using ResponseDispatcherType = ResponseDispatcher;
  using OnResponseCallbackType = ResponseDispatcher::OnResponseCallbackType;
  using OnErrorCallbackType = ResponseDispatcher::OnErrorCallbackType;

  using MessageHandlerCallbackType = std::function<void(
      EndpointType const &sender, BufferReader const &buffer)>;
  //@}

  /// @name Constructors, etc.
  //@{
  /*!
   * @brief Construct a Newtoek interface using the provided channels for
   * communication
   * over IPv4 and IPv6.
   *
   * @param [in] io_context for async operations, must be run by the caller.
   * @param [in] message_serializer the serializer used to binary encode
   * messages before they are sent.
   * @param [in] chan_ipv4 channel to be used for communication over IPv4.
   * @param [in] chan_ipv6 channel to be used for communication over IPv4.
   */
  Network(boost::asio::io_context &io_context,
      std::unique_ptr<TMessageSerializer> message_serializer,
      std::unique_ptr<TChannel> chan_ipv4,
      std::unique_ptr<TChannel> chan_ipv6 = {})
      : io_context_(io_context),
        message_serializer_(std::move(message_serializer)),
        chan_ipv4_(std::move(chan_ipv4)), chan_ipv6_(std::move(chan_ipv6)),
        response_dispatcher_(io_context) {
    ASLOG(debug, "Creating Network DONE at '{}' and '{}'",
        chan_ipv4_->LocalEndpoint().ToString(),
        chan_ipv6_ ? chan_ipv6_->LocalEndpoint().ToString() : "NO-IPV6");
  }

  /// Not copyable.
  Network(Network const &) = delete;
  /// Not copyable.
  auto operator=(Network const &) -> Network & = delete;

  /// Default trivial move constructor.
  Network(Network &&) noexcept = default;
  /// Default trivial assignment constructor.
  auto operator=(Network &&) noexcept -> Network & = default;

  ~Network() {
    ASLOG(debug, "Destroy Network");
  }
  //@}

  /*!
   * @brief Assign a handler for messages received through this Network
   * interface. Must
   * be called before Start() is called.
   *
   * To remove strong coupling between the different entities involved and
   * enable dependency injection for testing, the way the chain works is pretty
   * simple:
   *  - engine registers itself as message handler and starts the network.
   *  - every time a message is received, engine handler is called.
   *  - if the engine finds out the message is a response, it sends it back
   *    to the network for response dispatching.
   *    - network dispatches response as appropriate.
   *  - if request message, engine processes based on the request type.
   *
   * @param [in] handler callback handler to be invoked every time a message is
   * received. It cannot be null.
   */
  void OnMessageReceived(MessageHandlerCallbackType handler) {
    // ASAP_ASSERT(handler != nullptr);
    receive_handler_ = std::move(handler);
  }

  /// Start receiving messsage from the network. MUST be called after a message
  /// handler is registered through OnMessageReceived(handler).
  /// Calling Start() multiple times has no additional effect than the first
  /// time.
  void Start() {
    // ASAP_ASSERT(receive_handler_ != nullptr);
    static auto started = false;
    if (!started) {
      ScheduleReceive(*chan_ipv4_);
      if (chan_ipv6_) {
        ScheduleReceive(*chan_ipv6_);
      }
      started = true;
    }
  }

  /*!
   * @brief Called by the Engine (registered as the message handler for this
   * network) when the message being handled is a response message.
   *
   * This method is part of processing flow described in OnMessageReceived().
   * This should allow the Network to properly correlate responses to previous
   * requests and dispatch them to the registered response handlers.
   *
   * @param [in] source endpoint who sent this response message.
   * @param [in] header header of the response message (containing the
   * response/request correlation id)
   * @param [in] buffer a span over the range of bytes representing the response
   * data.
   */
  void HandleNewResponse(EndpointType const &source, Header const &header,
      BufferReader const &buffer) {
    response_dispatcher_.HandleResponse(source, header, buffer);
  }

  /*!
   * Resolution of a host and service names into a range of endpoint entries.
   *
   * This method is used to resolve host and service names into a list of
   * endpoint entries.
   *
   * @param [in] host A string identifying a location. May be a descriptive name
   * or a numeric address string. If an empty string, the resolved endpoints
   * will use the loopback address.
   * @param [in] service A string identifying the requested service. This may be
   * a descriptive name or a numeric string corresponding to a port number. May
   * be an empty string, in which case all resolved endpoints will have a port
   * number of 0.
   * @return A list of endpoint entries. A successful call to this function is
   * guaranteed to return a non-empty range.
   */
  auto ResolveEndpoint(std::string const &host, std::string const &service)
      -> std::vector<EndpointType> {
    return ChannelType::ResolveEndpoint(io_context_, host, service);
  }

  /*!
   * Send a 'conversational' request, i.e. a request that expects a response
   * back.
   *
   * This method is used to send request messages which expect a response
   * message back. Requests and responses are associated through a randomly
   * generated hash (id). The request message will carry that id and the
   * response must echo it.
   *
   * The caller can specify a time duration to wait for the response before
   * timing out.  Any response message related to the same request id that comes
   * after the timeout would be simply discarded.
   *
   * @tparam Request request message type.
   * @param [in] request request message to send.
   * @param [in] destination endpoint to which the request will be sent.
   * @param [in] timeout duration after which the error callback is invoked and
   * the wait for the response is cancelled.
   * @param [in] on_response_received callback to be invoked if a response
   * message for the request is received before the timeout duration expires.
   * @param [in] on_error callback invoked if a timeout occurs before any
   * response is received or to notify the user of other errors that occur while
   * waiting for the response.
   */
  template <typename Request>
  void SendConvRequest(Request const &request, EndpointType const &destination,
      Timer::DurationType const &timeout,
      OnResponseCallbackType const &on_response_received,
      OnErrorCallbackType const &on_error) {
    ASLOG(debug, "SendConvRequest to {}", destination);
    // Generate a random request/response id
    auto const correlation_id = blocxxi::crypto::Hash160::RandomHash();
    // Generate the request Buffer.
    auto message = message_serializer_->Serialize(request, correlation_id);
    ASLOG(trace, "  message serialized");

    auto on_message_sent = [this, correlation_id, on_response_received,
                               on_error,
                               timeout](std::error_code const &failure) {
      // If the send fails, report to the caller.
      if (failure) {
        ASLOG(debug, "  message send failed");
        on_error(failure);
      } else {
        ASLOG(trace, "  message sent");
        // Register the response/error callbacks for later dispatch
        // of the response message.
        response_dispatcher_.RegisterCallbackWithTimeout(
            correlation_id, timeout, on_response_received, on_error);
      }
    };

    SendMessage(message, destination, on_message_sent);
  }

  /*!
   * @brief Send a 'uni-directional' request, i.e. a request that does not
   * expect a response back.
   *
   * @tparam Request request message type.
   * @param [in] request request message to send.
   * @param [in] destination endpoint to which the request will be sent.
   */
  template <typename Request>
  void SendUniRequest(Request const &request, EndpointType const &destination) {
    auto const correlation_id = blocxxi::crypto::Hash160::RandomHash();
    auto message = message_serializer_->Serialize(request, correlation_id);
    SendMessage(message, destination, [](std::error_code const &failure) {
      if (failure) {
        ASLOG(debug, "failed to send uni-request: {}", failure.message());
      }
    });
  }

  /*!
   * @brief Send a response message to a destination endpoint.
   *
   * @tparam Response response message type.
   * @param [in] correlation_id id used to correlate between requests and
   * responses. This should be echoed from the request message corresponding to
   * this response.
   * @param [in] response response message.
   * @param [in] destination endpoint to which the response will be sent.
   */
  template <typename Response>
  void SendResponse(blocxxi::crypto::Hash160 const &correlation_id,
      Response const &response, EndpointType const &destination) {
    auto message = message_serializer_->Serialize(response, correlation_id);
    SendMessage(message, destination, [](std::error_code const &failure) {
      if (failure) {
        ASLOG(debug, "failed to send response: {}", failure.message());
      } else {
        ASLOG(debug, "response sent");
      }
    });
  }

private:
  auto GetChannelFor(IpEndpoint const &endpoint) -> TChannel & {
    return (endpoint.address_.is_v4()) ? *chan_ipv4_ : *chan_ipv6_;
  }

  template <typename Message, typename OnMessageSent>
  void SendMessage(Message const &message, IpEndpoint const &destination,
      OnMessageSent const &on_message_sent) {
    GetChannelFor(destination).AsyncSend(message, destination, on_message_sent);
  }

  void ScheduleReceive(TChannel &chan) {
    chan.AsyncReceive(
        [this, &chan](std::error_code const &failure, IpEndpoint const &sender,
            BufferReader const &buffer) {
          // Reception failures are fatal.
          if (!failure) {
            receive_handler_(sender, buffer);
          } else {
            ASLOG(error, "{}", failure.message());
            // TODO(Abdessattar): figure out how to handle receive errors if
            // needed throw std::system_error{failure};
          }
          ScheduleReceive(chan);
        });
  }

  /// Message serializer type
  using MessageSerializerType = TMessageSerializer;

  /// io_context used for the name resolution. Must be run by the caller.
  boost::asio::io_context &io_context_;
  /// The message serializer used to serialize messages before sending them.
  std::unique_ptr<MessageSerializerType> message_serializer_;
  /// The underlying IPv4 message channel.
  std::unique_ptr<TChannel> chan_ipv4_;
  /// The underlying IPv6 message channel.
  std::unique_ptr<TChannel> chan_ipv6_;
  /// The response dispatcher used to dispatch response messages to their
  /// associated handlers based on the request/response correlation id.
  ResponseDispatcher response_dispatcher_;
  /// Handler callback to be invoked every time a message is received.
  MessageHandlerCallbackType receive_handler_{nullptr};
};

} // namespace blocxxi::p2p::kademlia
