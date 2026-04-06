//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include <Blocxxi/Codec/bencode.h>
#include <Blocxxi/P2P/api_export.h>
#include <Blocxxi/P2P/kademlia/endpoint.h>
#include <Blocxxi/P2P/kademlia/node.h>

namespace blocxxi::p2p::kademlia {

struct BLOCXXI_P2P_API KrpcError final {
  std::int64_t code_ {};
  std::string message_;
};

struct BLOCXXI_P2P_API KrpcQuery final {
  std::string method_;
  blocxxi::codec::bencode::Value::DictionaryType arguments_;
};

struct BLOCXXI_P2P_API KrpcResponse final {
  blocxxi::codec::bencode::Value::DictionaryType values_;
};

struct BLOCXXI_P2P_API KrpcMessage final {
  enum class Type : std::uint8_t {
    Query,
    Response,
    Error,
  };

  using Payload = std::variant<KrpcQuery, KrpcResponse, KrpcError>;

  std::string transaction_id_;
  std::optional<std::string> version_;
  Payload payload_;
};

[[nodiscard]] BLOCXXI_P2P_API auto operator==(
  KrpcError const& lhs, KrpcError const& rhs) -> bool;
[[nodiscard]] BLOCXXI_P2P_API auto operator==(
  KrpcQuery const& lhs, KrpcQuery const& rhs) -> bool;
[[nodiscard]] BLOCXXI_P2P_API auto operator==(
  KrpcResponse const& lhs, KrpcResponse const& rhs) -> bool;
[[nodiscard]] BLOCXXI_P2P_API auto operator==(
  KrpcMessage const& lhs, KrpcMessage const& rhs) -> bool;

[[nodiscard]] BLOCXXI_P2P_API auto GetType(KrpcMessage const& message)
  -> KrpcMessage::Type;

[[nodiscard]] BLOCXXI_P2P_API auto Encode(KrpcMessage const& message)
  -> std::string;
[[nodiscard]] BLOCXXI_P2P_API auto DecodeKrpc(std::string_view encoded)
  -> KrpcMessage;

[[nodiscard]] BLOCXXI_P2P_API auto EncodeCompactPeer(IpEndpoint const& endpoint)
  -> std::string;
[[nodiscard]] BLOCXXI_P2P_API auto DecodeCompactPeer(std::string_view encoded)
  -> IpEndpoint;

[[nodiscard]] BLOCXXI_P2P_API auto EncodeCompactNode(Node const& node)
  -> std::string;
[[nodiscard]] BLOCXXI_P2P_API auto DecodeCompactNode(std::string_view encoded)
  -> Node;

} // namespace blocxxi::p2p::kademlia
