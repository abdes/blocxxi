//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <p2p/blocxxi_p2p_export.h>
#include <p2p/kademlia/message.h>

namespace blocxxi::p2p::kademlia {

/*!
 * @brief Offers serialization helpers for the Kademlia protocol messages.
 */
class BLOCXXI_P2P_API MessageSerializer {
public:
  /// @name Constructors etc.
  //@{
  /*!
   * @brief Create a MessageSerializer instance which will use the given node
   * id as this node's id during message header serialization.
   *
   * @param my_id this node's id, which will be automatically added to headers.
   */
  explicit MessageSerializer(Node::IdType my_id);

  /// Not copyable
  MessageSerializer(MessageSerializer const &) = delete;
  /// Not copyable
  auto operator=(MessageSerializer const &) -> MessageSerializer & = delete;

  /// Movable (default)
  MessageSerializer(MessageSerializer &&) noexcept = default;
  /// Movable (default)
  auto operator=(MessageSerializer &&) noexcept
      -> MessageSerializer & = default;

  ~MessageSerializer() = default;
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
  auto Serialize(TMessage const &message, blocxxi::crypto::Hash160 const &token)
      -> Buffer;

  /*!
   * Serialize a Kademlia protocol message header for the given message type.
   *
   * @param type message type.
   * @param token the random correlation token to be used in the header.
   * @return a buffer containing the serialized message header.
   */
  auto Serialize(Header::MessageType const &type,
      blocxxi::crypto::Hash160 const &token) -> Buffer;

private:
  /*!
   * Make a header for the given message type and using the random correlation
   * token.
   *
   * @param type message type.
   * @param token the random correlation token to be used in the header.
   * @return a fully populated Header object.
   */
  auto MakeHeader(Header::MessageType const &type,
      blocxxi::crypto::Hash160 const &token) -> Header;

  /// This node's id, used in serialized headers.
  Node::IdType my_id_;
};

template <typename TMessage>
inline auto MessageSerializer::Serialize(
    TMessage const &message, blocxxi::crypto::Hash160 const &token) -> Buffer {
  auto const type = MessageTraits<TMessage>::TYPE_ID;
  auto const header = MakeHeader(type, token);

  Buffer buffer;
  blocxxi::p2p::kademlia::Serialize(header, buffer);
  blocxxi::p2p::kademlia::Serialize(message, buffer);

  return buffer;
}

} // namespace blocxxi::p2p::kademlia
