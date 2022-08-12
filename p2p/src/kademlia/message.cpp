//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <p2p/kademlia/message.h>

#include <iostream>
#include <vector>

// #include <common/logging.h>

#include <p2p/kademlia/buffer.h>
#include <p2p/kademlia/detail/error_impl.h>

namespace blocxxi::p2p::kademlia {

namespace {

template <typename IntegerType>
inline void SerializeInteger(IntegerType value, Buffer &b) {
  // Cast the integer as unsigned for right shifting.
  using unsigned_integer_type = typename std::make_unsigned<IntegerType>::type;

  // ASLOG_MISC(debug, "serialize integer {}", value);
  for (auto i = 0U; i < sizeof(value); ++i) {
    // ASLOG_MISC(debug, "  write one byte: {}", Buffer::value_type(value));
    b.push_back(static_cast<Buffer::value_type>(value));
    static_cast<unsigned_integer_type &>(value) >>= 8;
  }
}

template <typename IntegerType>
auto DeserializeInteger(BufferReader const &buffer, IntegerType &value)
    -> std::size_t {
  value = 0;
  // ASLOG_MISC(debug, "deserialize integer");

  if (buffer.size_bytes() < sizeof(value)) {
    throw std::range_error("insufficient data in input buffer");
  }
  auto data = std::cbegin(buffer);
  for (auto ii = 0U; ii < sizeof(value); ++ii) {
    // ASLOG_MISC(debug, "  read one byte: {}", *data);
    value |= IntegerType{*data++} << 8 * ii;
  }

  return sizeof(value);
}

inline void Serialize(std::vector<std::uint8_t> data, Buffer &b) {
  SerializeInteger(data.size(), b);
  b.insert(b.end(), data.begin(), data.end());
}

inline auto Deserialize(BufferReader const &buffer,
    std::vector<std::uint8_t> &data) -> std::size_t {
  std::vector<std::uint8_t>::size_type size = 0;
  auto consumed = DeserializeInteger(buffer, size);
  auto sub = buffer.subspan(consumed);
  if (sub.size_bytes() < size) {
    throw std::range_error("insufficient data in input buffer");
  }
  data.insert(data.end(), std::cbegin(sub), std::cend(sub));
  consumed += size;
  return consumed;
}

inline void Serialize(blocxxi::crypto::Hash160 const &i, Buffer &b) {
  // ASLOG_MISC(debug, "serialize hash 160");
  b.insert(b.end(), i.begin(), i.end());
}

inline auto Deserialize(BufferReader const &buffer,
    blocxxi::crypto::Hash160 &new_id) -> std::size_t {
  if (buffer.size_bytes() < blocxxi::crypto::Hash160::Size()) {
    throw std::range_error("insufficient data in input buffer");
  }
  // ASLOG_MISC(debug, "deserialize hash 160");
  std::copy_n(buffer.begin(), blocxxi::crypto::Hash160::Size(), new_id.begin());

  return blocxxi::crypto::Hash160::Size();
}

inline auto Deserialize(BufferReader const &buffer, Header::Version &version,
    Header::MessageType &message_type) -> std::size_t {
  if (buffer.size_bytes() < 1) {
    throw std::range_error("insufficient data in input buffer");
  }
  auto first_byte = *std::cbegin(buffer);
  // ASLOG_MISC(debug, "deserialize header version/type {}", first_byte);
  version = static_cast<Header::Version>(first_byte >> 4);
  message_type = static_cast<Header::MessageType>(first_byte & 0xf);

  if (version != Header::Version::V1) {
    throw std::runtime_error("unknown protocol version");
  }

  return 1;
}

/// Constants representing the encoded address type.
enum {
  KADEMLIA_ENDPOINT_SERIALIZATION_IPV4 = 1,
  KADEMLIA_ENDPOINT_SERIALIZATION_IPV6 = 2
};

inline void Derialize(boost::asio::ip::address const &address, Buffer &b) {
  // ASLOG_MISC(debug, "serialize address {} is_v4={}", address.to_string(),
  //           address.is_v4());
  if (address.is_v4()) {
    b.push_back(KADEMLIA_ENDPOINT_SERIALIZATION_IPV4);
    auto const &a = address.to_v4().to_bytes();
    b.insert(b.end(), a.begin(), a.end());
  } else {
    // ASAP_ASSERT(address.is_v6() && "unknown IP version");
    b.push_back(KADEMLIA_ENDPOINT_SERIALIZATION_IPV6);
    auto const &a = address.to_v6().to_bytes();
    b.insert(b.end(), a.begin(), a.end());
  }
}

/**
 *
 */
template <typename Address>
inline auto DeserializeAddress(BufferReader const &buffer, Address &address)
    -> std::size_t {
  typename Address::bytes_type address_bytes;
  if (buffer.size_bytes() < address_bytes.size()) {
    throw std::range_error("insufficient data in input buffer");
  }

  std::copy_n(
      std::cbegin(buffer), address_bytes.size(), std::begin(address_bytes));

  address = Address{address_bytes};

  return address_bytes.size();
}

inline auto Deserialize(BufferReader const &buffer,
    boost::asio::ip::address &address) -> std::size_t {
  if (buffer.size_bytes() < 1) {
    throw std::range_error("insufficient data in input buffer");
  }

  auto const protocol = buffer[0];
  std::size_t consumed = 1;
  // ASLOG_MISC(debug, "deserialize address protocol={}", protocol);
  if (protocol == KADEMLIA_ENDPOINT_SERIALIZATION_IPV4) {
    boost::asio::ip::address_v4 a;
    consumed += DeserializeAddress(buffer.subspan(consumed), a);

    address = a;
  } else {
    // ASAP_ASSERT_VAL((protocol == KADEMLIA_ENDPOINT_SERIALIZATION_IPV6 &&
    //                     "unknown IP version"),
    //                    (int)protocol);

    boost::asio::ip::address_v6 a;
    consumed += DeserializeAddress(buffer.subspan(consumed), a);

    address = a;
  }

  return consumed;
}

inline void Serialize(Node const &node, Buffer &buffer) {
  Serialize(node.Id(), buffer);
  std::uint16_t port = node.Endpoint().Port();
  SerializeInteger(port, buffer);
  Derialize(node.Endpoint().Address(), buffer);
}

inline auto Deserialize(BufferReader const &buffer, Node &node) -> std::size_t {
  auto consumed = Deserialize(buffer, node.Id());
  std::uint16_t port;
  consumed += DeserializeInteger(buffer.subspan(consumed), port);
  node.Endpoint().Port(port);
  IpEndpoint::AddressType address;
  consumed += Deserialize(buffer.subspan(consumed), address);
  node.Endpoint().Address(address);
  return consumed;
}

} // anonymous namespace

inline auto operator<<(std::ostream &out,
    Header::MessageType const &message_type) -> std::ostream & {
  switch (message_type) {
  case Header::MessageType::PING_REQUEST:
    return out << "ping_request";
  case Header::MessageType::PING_RESPONSE:
    return out << "ping_response";
  case Header::MessageType::STORE_REQUEST:
    return out << "store_request";
  case Header::MessageType::FIND_NODE_REQUEST:
    return out << "find_peer_request";
  case Header::MessageType::FIND_NODE_RESPONSE:
    return out << "find_peer_response";
  case Header::MessageType::FIND_VALUE_REQUEST:
    return out << "find_value_request";
  case Header::MessageType::FIND_VALUE_RESPONSE:
    return out << "find_value_response";
  }
  return out << "unknown_message";
}

inline auto operator<<(std::ostream &out, Header const &header)
    -> std::ostream & {
  return out << header.type_;
}

void Serialize(Header const &h, Buffer &b) {
  // ASLOG_MISC(debug, "serialize header version/type");
  b.push_back(static_cast<uint8_t>(ToUnderlying(h.version_) << 4) |
              ToUnderlying(h.type_));
  Serialize(h.source_id_, b);
  Serialize(h.random_token_, b);
}

auto Deserialize(BufferReader const &buffer, Header &header) -> std::size_t {
  // ASLOG_MISC(debug, "deserialize header");
  auto consumed = Deserialize(buffer, header.version_, header.type_);
  consumed += Deserialize(buffer.subspan(consumed), header.source_id_);
  consumed += Deserialize(buffer.subspan(consumed), header.random_token_);
  return consumed;
}

void Serialize(FindNodeRequestBody const &body, Buffer &b) {
  Serialize(body.node_id_, b);
}

auto Deserialize(BufferReader const &buffer, FindNodeRequestBody &body)
    -> std::size_t {
  return Deserialize(buffer, body.node_id_);
}

void Serialize(FindNodeResponseBody const &body, Buffer &b) {
  std::size_t size = body.peers_.size();
  SerializeInteger(size, b);

  for (auto const &n : body.peers_) {
    Serialize(n, b);
  }
}

auto Deserialize(BufferReader const &buffer, FindNodeResponseBody &body)
    -> std::size_t {
  std::size_t size = 0;
  auto consumed = DeserializeInteger(buffer, size);

  for (; size > 0; --size) {
    body.peers_.resize(body.peers_.size() + 1);
    consumed += Deserialize(buffer.subspan(consumed), body.peers_.back());
  }

  return consumed;
}

void Serialize(FindValueRequestBody const &body, Buffer &b) {
  Serialize(body.value_key_, b);
}

auto Deserialize(BufferReader const &buffer, FindValueRequestBody &body)
    -> std::size_t {
  return Deserialize(buffer, body.value_key_);
}

void Serialize(FindValueResponseBody const &body, Buffer &b) {
  Serialize(body.data_, b);
}

auto Deserialize(BufferReader const &buffer, FindValueResponseBody &body)
    -> std::size_t {
  return Deserialize(buffer, body.data_);
}

void Serialize(StoreValueRequestBody const &body, Buffer &b) {
  Serialize(body.data_key_, b);

  Serialize(body.data_value_, b);
}

auto Deserialize(BufferReader const &buffer, StoreValueRequestBody &body)
    -> std::size_t {
  auto consumed = Deserialize(buffer, body.data_key_);
  consumed += Deserialize(buffer.subspan(consumed), body.data_value_);
  return consumed;
}

} // namespace blocxxi::p2p::kademlia
