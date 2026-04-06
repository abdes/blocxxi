//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/P2P/kademlia/krpc.h>

#include <array>
#include <stdexcept>

namespace blocxxi::p2p::kademlia {

namespace {

using BencodeValue = blocxxi::codec::bencode::Value;

auto RequireDictionary(BencodeValue const& value) -> BencodeValue::DictionaryType const&
{
  return value.AsDictionary();
}

auto RequireList(BencodeValue const& value) -> BencodeValue::ListType const&
{
  return value.AsList();
}

auto RequireString(
  BencodeValue::DictionaryType const& dictionary, std::string_view key)
  -> std::string const&
{
  auto found = dictionary.find(key);
  if (found == dictionary.end()) {
    throw std::invalid_argument("missing KRPC string field");
  }
  return found->second.AsString();
}

auto RequireDictionaryField(
  BencodeValue::DictionaryType const& dictionary, std::string_view key)
  -> BencodeValue::DictionaryType const&
{
  auto found = dictionary.find(key);
  if (found == dictionary.end()) {
    throw std::invalid_argument("missing KRPC dictionary field");
  }
  return found->second.AsDictionary();
}

} // namespace

auto operator==(KrpcError const& lhs, KrpcError const& rhs) -> bool
{
  return lhs.code_ == rhs.code_ && lhs.message_ == rhs.message_;
}

auto operator==(KrpcQuery const& lhs, KrpcQuery const& rhs) -> bool
{
  return lhs.method_ == rhs.method_ && lhs.arguments_ == rhs.arguments_;
}

auto operator==(KrpcResponse const& lhs, KrpcResponse const& rhs) -> bool
{
  return lhs.values_ == rhs.values_;
}

auto operator==(KrpcMessage const& lhs, KrpcMessage const& rhs) -> bool
{
  return lhs.transaction_id_ == rhs.transaction_id_
    && lhs.version_ == rhs.version_ && lhs.payload_ == rhs.payload_;
}

auto GetType(KrpcMessage const& message) -> KrpcMessage::Type
{
  if (std::holds_alternative<KrpcQuery>(message.payload_)) {
    return KrpcMessage::Type::Query;
  }
  if (std::holds_alternative<KrpcResponse>(message.payload_)) {
    return KrpcMessage::Type::Response;
  }
  return KrpcMessage::Type::Error;
}

auto Encode(KrpcMessage const& message) -> std::string
{
  auto root = BencodeValue::DictionaryType {};
  root.emplace("t", BencodeValue(message.transaction_id_));
  if (message.version_) {
    root.emplace("v", BencodeValue(*message.version_));
  }

  switch (GetType(message)) {
  case KrpcMessage::Type::Query: {
    auto const& query = std::get<KrpcQuery>(message.payload_);
    root.emplace("y", BencodeValue("q"));
    root.emplace("q", BencodeValue(query.method_));
    root.emplace("a", BencodeValue(query.arguments_));
    break;
  }
  case KrpcMessage::Type::Response: {
    auto const& response = std::get<KrpcResponse>(message.payload_);
    root.emplace("y", BencodeValue("r"));
    root.emplace("r", BencodeValue(response.values_));
    break;
  }
  case KrpcMessage::Type::Error: {
    auto const& error = std::get<KrpcError>(message.payload_);
    root.emplace("y", BencodeValue("e"));
    root.emplace("e",
      BencodeValue(BencodeValue::ListType {
        BencodeValue(error.code_),
        BencodeValue(error.message_),
      }));
    break;
  }
  }

  return blocxxi::codec::bencode::Encode(BencodeValue(root));
}

auto DecodeKrpc(std::string_view encoded) -> KrpcMessage
{
  auto const root_value = blocxxi::codec::bencode::Decode(encoded);
  auto const& root = RequireDictionary(root_value);

  auto const& type = RequireString(root, "y");
  auto message = KrpcMessage {};
  message.transaction_id_ = RequireString(root, "t");
  if (auto found = root.find("v"); found != root.end()) {
    message.version_ = found->second.AsString();
  }

  if (type == "q") {
    message.payload_ = KrpcQuery {
      RequireString(root, "q"),
      RequireDictionaryField(root, "a"),
    };
    return message;
  }

  if (type == "r") {
    message.payload_ = KrpcResponse { RequireDictionaryField(root, "r") };
    return message;
  }

  if (type == "e") {
    auto found = root.find("e");
    if (found == root.end()) {
      throw std::invalid_argument("missing KRPC error field");
    }
    auto const& error = RequireList(found->second);
    if (error.size() != 2) {
      throw std::invalid_argument("KRPC error must contain code and message");
    }
    message.payload_ = KrpcError {
      error.at(0).AsInteger(),
      error.at(1).AsString(),
    };
    return message;
  }

  throw std::invalid_argument("unknown KRPC message type");
}

auto EncodeCompactPeer(IpEndpoint const& endpoint) -> std::string
{
  if (!endpoint.Address().is_v4()) {
    throw std::invalid_argument("compact peer encoding currently requires IPv4");
  }

  auto const address = endpoint.Address().to_v4().to_bytes();
  auto out = std::string();
  out.reserve(6);
  out.append(reinterpret_cast<char const*>(address.data()), address.size());
  out.push_back(static_cast<char>((endpoint.Port() >> 8) & 0xffU));
  out.push_back(static_cast<char>(endpoint.Port() & 0xffU));
  return out;
}

auto DecodeCompactPeer(std::string_view encoded) -> IpEndpoint
{
  if (encoded.size() != 6) {
    throw std::invalid_argument("compact peer encoding must be 6 bytes");
  }

  auto address_bytes = asio::ip::address_v4::bytes_type {};
  std::copy_n(encoded.begin(), 4, address_bytes.begin());
  auto const port = static_cast<std::uint16_t>(
    (static_cast<std::uint8_t>(encoded[4]) << 8)
    | static_cast<std::uint8_t>(encoded[5]));

  return IpEndpoint(asio::ip::address_v4(address_bytes), port);
}

auto EncodeCompactNode(Node const& node) -> std::string
{
  if (!node.Endpoint().Address().is_v4()) {
    throw std::invalid_argument("compact node encoding currently requires IPv4");
  }

  auto out = std::string();
  out.reserve(26);
  out.append(reinterpret_cast<char const*>(node.Id().Data()), node.Id().Size());
  out.append(EncodeCompactPeer(node.Endpoint()));
  return out;
}

auto DecodeCompactNode(std::string_view encoded) -> Node
{
  if (encoded.size() != 26) {
    throw std::invalid_argument("compact node encoding must be 26 bytes");
  }

  auto id = Node::IdType {};
  std::copy_n(encoded.begin(), id.Size(), id.begin());
  auto endpoint = DecodeCompactPeer(encoded.substr(id.Size()));
  return Node(std::move(id), std::move(endpoint));
}

} // namespace blocxxi::p2p::kademlia
