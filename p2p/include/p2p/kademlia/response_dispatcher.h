//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <common/logging.h>
#include <p2p/kademlia/buffer.h>
#include <p2p/kademlia/endpoint.h>
#include <p2p/kademlia/key.h>
#include <p2p/kademlia/message.h>
#include <p2p/kademlia/timer.h>

#include <p2p/kademlia/detail/error_impl.h>

namespace blocxxi {
namespace p2p {
namespace kademlia {

/*!
 * @brief ResponseDispatcher manages the association between response callbacks
 * and awaited response messages.
 *
 * The RPC protocol is simple. A single query packet is sent out and a single
 * packet is sent in response. The response is associated to the request through
 * a id (Hash160) sent in the response packet and must be echoed in the response
 * packet.
 *
 * Using the request/response id, a callback for handling the response and a
 * callback for handling errors can be registered with the ResponseDispatcher.
 *
 * To avoid waiting indefinitely for a never coming response, the callback
 * registration is made only for a certain duration after which the callback is
 * removed. Any received response with an id for which no callback is registered
 * is simply discarded.
 */
class ResponseDispatcher final
    : asap::logging::Loggable<asap::logging::Id::P2P_KADEMLIA> {
 public:
  /// @name Type shortcuts
  //@{
  using EndpointType = IpEndpoint;

  using OnResponseCallbackType = std::function<void(
      EndpointType const &sender, Header const &h, BufferReader const &buffer)>;

  using OnErrorCallbackType = std::function<void(std::error_code)>;
  //@}

 public:
  /// @name Constructors, etc.
  //@{
  explicit ResponseDispatcher(boost::asio::io_context &io_context)
      : callbacks_(), timer_(io_context) {
    ASLOG(debug, "Creating ResponseDispatcher DONE");
  }

  ResponseDispatcher(ResponseDispatcher const &) = delete;
  ResponseDispatcher &operator=(ResponseDispatcher const &) = delete;

  ResponseDispatcher(ResponseDispatcher &&) = default;
  ResponseDispatcher &operator=(ResponseDispatcher &&) = default;

  ~ResponseDispatcher() { ASLOG(debug, "Destroy ResponseDispatcher"); }
  //@}

  /*!
   * @brief Handle an incoming response by trying to dispatch it to its
   * registered callback if we have any.
   *
   * A response with no registered callback is simply discarded.
   *
   * @param [in] sender the endpoint from which this response originated.
   * @param [in] header the response header.
   * @param [in] buffer a span over the range of bytes representing the response
   * data.
   */
  void HandleResponse(EndpointType const &sender, Header const &header,
                      BufferReader const &buffer) {
    // Try to dispatch the response to its registered callback.
    auto failure = DispatchResponse(sender, header, buffer);
    if (failure == UNASSOCIATED_MESSAGE_ID) {
      // Discard responses for which we have no registered callback
      ASLOG(debug, "dropping response with no registered callback");
    }
  }

  /*!
   * @brief Register a callback for a response id for a specified duration.
   *
   * The callback will be associated to the response id only for the specified
   * period of time as per callback_ttl. If no response is received within that
   * period of time, the callback is removed.
   *
   * @tparam OnResponseReceived type of the callback to be invoked when the
   * response is
   * received.
   * @tparam OnError type of the callback to be invoked if a an error occurs or
   * the timeout
   * period expires.
   * @param [in] response_id the response id for which this callback will be
   * registered.
   * @param [in] callback_ttl std::chrono duration for which the callback will
   * be held waiting for the response.
   * @param [in] on_response_received callback to be invoked when the response
   * is received.
   * @param [in] on_error callback to be invoked if an error occurs or the
   * timeout period expires.
   */
  void RegisterCallbackWithTimeout(
      KeyType const &response_id, Timer::DurationType const &callback_ttl,
      OnResponseCallbackType const &on_response_received,
      OnErrorCallbackType const &on_error) {
    ASLOG(trace, "  register response callback with timeout");
    // Associate the response id with the on_response_received callback.
    AddCallback(response_id, on_response_received);

    // Start the timer waiting for the response.
    timer_.ExpiresFromNow(callback_ttl, [this, on_error, response_id](void) {
      // If a callback is still in the map after the timeout, then it will
      // actually be removed by the next call to RemoveCallback indicating that
      // a response has never been received.
      if (RemoveCallback(response_id)) {
        ASLOG(trace, "  removed timed out response callback");
        ASLOG(trace, "  invoking error handler");
        on_error(make_error_code(std::errc::timed_out));
      }
    });
  }

 private:
  /*!
   * @brief Register a callback for the given response key.
   *
   * @param [in] response_id the response id for which this callback will be
   * registered.
   * @param [in] on_response_received callback to be invoked when the response
   * is received.
   */
  void AddCallback(KeyType const &response_id,
                   OnResponseCallbackType const &on_response_received) {
    // auto i = callbacks_.emplace(response_id, on_response_received);
    //(void)i;
    // assert(i.second && "an id can't be registered twice");
    ASLOG(debug, "  add callback to map {}", callbacks_.size());
    callbacks_.emplace(response_id, on_response_received);
  }

  /*!
   * @brief Remove the callback associated with the given response id. If no
   * callback is registered, a call to this method has no effect.
   *
   * @param [in] response_id the response id for which any registered callback
   * should
   * be removed.
   * @return \em true if a callback was found and removed; \em false otherwise.
   */
  bool RemoveCallback(KeyType const &response_id) {
    auto removed = callbacks_.erase(response_id);
    return (removed > 0);
  }

  /*!
   * Invoke the callback registered for the newly received response.
   *
   * If no callback is registered for the response id, an error is returned.
   *
   * @param sender the endpoint from which the response is originating.
   * @param header the response header (containing the response id)
   * @param buffer a span over the range of bytes representing the response
   * data.
   * @return 0 (no error) if the dispatch was successful; otherwise if no
   * callback was associated to the response id, UNASSOCIATED_MESSAGE_ID error.
   */
  std::error_code DispatchResponse(EndpointType const &sender,
                                   Header const &header,
                                   BufferReader const &buffer) {
    auto callback = callbacks_.find(header.random_token_);
    if (callback == callbacks_.end()) {
      return detail::make_error_code(UNASSOCIATED_MESSAGE_ID);
    }

    callback->second(sender, header, buffer);
    // It's always a one time call, so remove the callback after invoking it.
    callbacks_.erase(callback);
    ASLOG(debug, "  removed response callback");

    return std::error_code{};
  }

 private:
  /// The collection type for storing all registered response callbacks
  using CallbackCollectionType =
      std::map<blocxxi::crypto::Hash160, OnResponseCallbackType>;
  /// The collection of registered callbacks
  CallbackCollectionType callbacks_;
  ///
  Timer timer_;
};

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi
