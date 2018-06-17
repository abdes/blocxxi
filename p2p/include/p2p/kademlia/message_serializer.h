//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#ifndef BLOCXXI_P2P_KADEMLIA_MESSAGE_SERIALIZER_H_
#define BLOCXXI_P2P_KADEMLIA_MESSAGE_SERIALIZER_H_

#include <memory>

#include <p2p/kademlia/message.h>

namespace blocxxi {
namespace p2p {
namespace kademlia {

/*!
 * @brief Offers serialization helpers for the Kademlia protocol messages.
 */
class MessageSerializer {
 public:
  /// @name Constructors etc.
  //@{
  /*!
   * @brief Create a MessageSerializer instance which will use the given node
   * id as this node's id during message header serialization.
   *
   * @param my_id this node's id, which will be automatically added to headers.
   */
  MessageSerializer(Node::IdType const &my_id);

  /// Not copyable
  MessageSerializer(MessageSerializer const &) = delete;
  /// Not copyable
  MessageSerializer &operator=(MessageSerializer const &) = delete;

  /// Movable (default)
  MessageSerializer(MessageSerializer &&) = default;
  /// Movable (default)
  MessageSerializer &operator=(MessageSerializer &&) = default;
  //@}

  /*!
   * Serialize a Kademlia protocol message body.
   *
   * @tparam TMessage type of messages to serialize.
   * @param message the message body to serialize.
   * @param token the random correlation token to be used in the header.
   * @return a buffer containing the serialized message body.
   */
  template <typename TMessage>
  Buffer Serialize(TMessage const &message,
                   blocxxi::crypto::Hash160 const &token);

  /*!
   * Serialize a Kademlia protocol message header for the given message type.
   *
   * @param type message type.
   * @param token the random correlation token to be used in the header.
   * @return a buffer containing the serialized message header.
   */
  Buffer Serialize(Header::MessageType const &type,
                   blocxxi::crypto::Hash160 const &token);

 private:
  /*!
   * Make a header for the given message type and using the random correlation
   * token.
   *
   * @param type message type.
   * @param token the random correlation token to be used in the header.
   * @return a fully populated Header object.
   */
  Header MakeHeader(Header::MessageType const &type,
                    blocxxi::crypto::Hash160 const &token);

 private:
  /// This node's id, used in serialized headers.
  Node::IdType const my_id_;
};

template <typename TMessage>
inline Buffer MessageSerializer::Serialize(
    TMessage const &message, blocxxi::crypto::Hash160 const &token) {
  auto const type = MessageTraits<TMessage>::TYPE_ID;
  auto const header = MakeHeader(type, token);

  Buffer buffer;
  blocxxi::p2p::kademlia::Serialize(header, buffer);
  blocxxi::p2p::kademlia::Serialize(message, buffer);

  return buffer;
}

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi

#endif  // BLOCXXI_P2P_KADEMLIA_MESSAGE_SERIALIZER_H_
