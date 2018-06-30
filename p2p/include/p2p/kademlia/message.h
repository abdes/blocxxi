//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#pragma once

#include <cstdint>      // for std::uint8_t etc.
#include <type_traits>  // for std::underlying_type_t<>

#include <gsl/gsl>  // for gsl::span

#include <p2p/kademlia/buffer.h>
#include <p2p/kademlia/node.h>

namespace blocxxi {
namespace p2p {
namespace kademlia {

/*!
 * @brief Represents a message header block.
 *
 * In its serialized form, every message is composed of a header and a body. The
 * header contains the protocol version, the message body type, the ID (hash
 * 160) of the source node and a random token (hash 160) used to correlate
 * requests and responses.
 *
 * A serialized header uses the following layout:
 * ```
 *      <-----------------><-------...-------><-------...------->
 *            1 byte             20 bytes           20 bytes
 *        4 bits | 4 bits      source id          random token
 *        version  type
 *
 * ```
 */
struct Header final {
  /// Protocol version
  enum class Version : std::uint8_t {
    V1 = 1,
  } version_{Version::V1};

  /// Message body type
  enum class MessageType : std::uint8_t {
    PING_REQUEST,
    PING_RESPONSE,
    STORE_REQUEST,
    FIND_NODE_REQUEST,
    FIND_NODE_RESPONSE,
    FIND_VALUE_REQUEST,
    FIND_VALUE_RESPONSE,
  } type_{MessageType::PING_REQUEST};

  /// ID of the source node (Hash 160)
  Node::IdType source_id_;
  /// A random token (Hash 160)
  blocxxi::crypto::Hash160 random_token_;
};

/*!
 * @brief Generalized function that can take any scoped enumerator and return
 * a compile time constant of the underlying type.
 *
 * Used specifically for the serialization and deserialization of headers. For
 * more details on the rational see Scott Meyers, Effective Modern C++ Item 10.
 *
 * @tparam TEnum the scoped enum class type.
 * @param [in] e_value any value of the scoped enum class.
 * @return a compile time constant of the underlying type corresponding to
 * e_value.
 */
template <typename TEnum>
constexpr std::underlying_type_t<TEnum> ToUnderlying(TEnum e_value) noexcept {
  return static_cast<std::underlying_type_t<TEnum>>(e_value);
}

/*!
 * @brief A template trait class used to carry extra information about the
 * messages types in use.
 *
 * The traits class T<M> lets us record such extra information about a message
 * class M, without requiring any change at all to M. Such extra information is
 * used to generically serialize and deserialize messages.
 *
 * @tparam TMessageBody the type of the message body described by this traits
 * class.
 */
template <typename TMessageBody>
struct MessageTraits;

/*!
 * @brief Serialize the header into the given buffer, which is expected to be
 * able to accomodate the header binary size (41 bytes) or expand as needed.
 *
 * @param [in] header header object to be serialized.
 * @param [out] buffer destination buffer for the header serialized binary form.
 */
void Serialize(Header const &header, Buffer &buffer);

/*!
 * @brief Deserialize the contents of the buffer into a Header object.
 *
 * @param [in] buffer a span over a range of bytes (std::uint8_t) representing
 * the header binary data.
 * @param [out] header the deserialized resulting header object.
 * @return an error code indicating success or failure of the deserialization.
 */
std::size_t Deserialize(BufferReader const &buffer, Header &header);

/// FIND_NODE request message body.
struct FindNodeRequestBody final {
  /// The hash 160 id of the node to find.
  Node::IdType node_id_;
};

/// Message traits template specialization for the FIND_NODE request message.
template <>
struct MessageTraits<FindNodeRequestBody> {
  static constexpr Header::MessageType TYPE_ID =
      Header::MessageType::FIND_NODE_REQUEST;
};

/*!
 * @brief Serialize a FIND_NODE request body into the given buffer.
 *
 * @param [in] body the request body.
 * @param [out] buffer the destination buffer.
 */
void Serialize(FindNodeRequestBody const &body, Buffer &buffer);

/*!
 * @brief Deserialize a FIND_NODE request body from the given buffer.
 *
 * @param [in] buffer the input buffer.
 * @param [out] body the resulting request body.
 * @return the number of consumed bytes from the input buffer. Subsequent
 * deserialization from the buffer need to start after the consumed bytes.
 */
std::size_t Deserialize(BufferReader const &buffer, FindNodeRequestBody &body);

/// FIND_NODE response message body.
struct FindNodeResponseBody final {
  /// The list of nodes in the response.
  std::vector<Node> peers_;
};

/// Message traits template specialization for the FIND_NODE response message.
template <>
struct MessageTraits<FindNodeResponseBody> {
  static constexpr Header::MessageType TYPE_ID =
      Header::MessageType::FIND_NODE_RESPONSE;
};

/*!
 * @brief Serialize a FIND_NODE response body into the given buffer.
 *
 * @param [in] body the response body.
 * @param [out] buffer the destination buffer.
 */
void Serialize(FindNodeResponseBody const &body, Buffer &buffer);

/*!
 * @brief Deserialize a FIND_NODE response body from the given buffer.
 *
 * @param [in] buffer the input buffer.
 * @param [out] body the resulting response body.
 * @return the number of consumed bytes from the input buffer. Subsequent
 * deserialization from the buffer need to start after the consumed bytes.
 */
std::size_t Deserialize(BufferReader const &buffer, FindNodeResponseBody &body);

/// FIND_VALUE request message body.
struct FindValueRequestBody final {
  /// The hash 160 key of the value to find.
  blocxxi::crypto::Hash160 value_key_;
};

/// Message traits template specialization for the FIND_VALUE request message.
template <>
struct MessageTraits<FindValueRequestBody> {
  static constexpr Header::MessageType TYPE_ID =
      Header::MessageType::FIND_VALUE_REQUEST;
};

/*!
 * @brief Serialize a FIND_VALUE request body into the given buffer.
 *
 * @param [in] body the request body.
 * @param [out] buffer the destination buffer.
 */
void Serialize(FindValueRequestBody const &body, Buffer &buffer);

/*!
 * @brief Deserialize a FIND_VALUE request body from the given buffer.
 *
 * @param [in] buffer the input buffer.
 * @param [out] body the resulting request body.
 * @return the number of consumed bytes from the input buffer. Subsequent
 * deserialization from the buffer need to start after the consumed bytes.
 */
std::size_t Deserialize(BufferReader const &buffer, FindValueRequestBody &body);

/// FIND_VALUE response message body.
struct FindValueResponseBody final {
  /// The value data  as a vector of bytes.
  std::vector<std::uint8_t> data_;
};

/// Message traits template specialization for the FIND_VALUE response message.
template <>
struct MessageTraits<FindValueResponseBody> {
  static constexpr Header::MessageType TYPE_ID =
      Header::MessageType::FIND_VALUE_RESPONSE;
};

/*!
 * @brief Serialize a FIND_VALUE response body into the given buffer.
 *
 * @param [in] body the response body.
 * @param [out] buffer the destination buffer.
 */
void Serialize(FindValueResponseBody const &body, Buffer &buffer);

/*!
 * @brief Deserialize a FIND_VALUE response body from the given buffer.
 *
 * @param [in] buffer the input buffer.
 * @param [out] body the resulting response body.
 * @return the number of consumed bytes from the input buffer. Subsequent
 * deserialization from the buffer need to start after the consumed bytes.
 */
std::size_t Deserialize(BufferReader const &buffer,
                        FindValueResponseBody &body);

/// STORE_VALUE request message body.
struct StoreValueRequestBody final {
  /// The hash 160 of the data key.
  blocxxi::crypto::Hash160 data_key_;
  /// The data value as a vector of bytes.
  std::vector<std::uint8_t> data_value_;
};

/// Message traits template specialization for the STORE_VALUE request message.
template <>
struct MessageTraits<StoreValueRequestBody> {
  static constexpr Header::MessageType TYPE_ID =
      Header::MessageType::STORE_REQUEST;
};

/*!
 * @brief Serialize a STORE_VALUE request body into the given buffer.
 *
 * @param [in] body the request body.
 * @param [out] buffer the destination buffer.
 */
void Serialize(StoreValueRequestBody const &body, Buffer &buffer);

/*!
 * @brief Deserialize a STORE_VALUE request body from the given buffer.
 *
 * @param [in] buffer the input buffer.
 * @param [out] body the resulting request body.
 * @return the number of consumed bytes from the input buffer. Subsequent
 * deserialization from the buffer need to start after the consumed bytes.
 */
std::size_t Deserialize(BufferReader const &buffer,
                        StoreValueRequestBody &body);

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi
