//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Blocxxi/Core/api_export.h>

#include <cstdint>
#include <string>
#include <vector>

#include <Blocxxi/Core/primitives.h>
#include <Blocxxi/Crypto/keypair.h>

namespace blocxxi::core {

struct EventAttribute {
  std::string key {};
  std::string value {};

  friend auto operator==(EventAttribute const& lhs, EventAttribute const& rhs)
    -> bool = default;
};

struct EventWindow {
  std::int64_t start_utc { 0 };
  std::int64_t end_utc { 0 };

  friend auto operator==(EventWindow const& lhs, EventWindow const& rhs)
    -> bool = default;
};

struct EventEnvelope {
  std::string schema { "blocxxi.event.v1" };
  std::string event_type {};
  std::string taxonomy {};
  std::string source {};
  std::string producer {};
  EventWindow window {};
  std::int64_t observed_at_utc { 0 };
  std::int64_t published_at_utc { 0 };
  std::vector<std::string> identifiers {};
  std::vector<EventAttribute> attributes {};
  std::string summary {};
  ByteVector payload {};

  [[nodiscard]] BLOCXXI_CORE_API auto CanonicalText() const -> std::string;
  [[nodiscard]] BLOCXXI_CORE_API auto CanonicalBytes() const -> ByteVector;
  [[nodiscard]] BLOCXXI_CORE_API auto ContentId() const -> blocxxi::crypto::Hash256;

  friend auto operator==(EventEnvelope const& lhs, EventEnvelope const& rhs)
    -> bool = default;
};

struct SignedEventRecord {
  EventEnvelope envelope {};
  std::string signer_name {};
  std::string signer_public_key_hex {};
  std::string signature_hex {};

  [[nodiscard]] BLOCXXI_CORE_API auto DeterministicKey() const -> std::string;

  friend auto operator==(SignedEventRecord const& lhs, SignedEventRecord const& rhs)
    -> bool = default;
};

[[nodiscard]] BLOCXXI_CORE_API auto DeriveDeterministicEventKey(
  EventEnvelope const& envelope) -> std::string;

[[nodiscard]] BLOCXXI_CORE_API auto SignEventRecord(EventEnvelope envelope,
  blocxxi::crypto::KeyPair const& key_pair, std::string signer_name = {})
  -> SignedEventRecord;

[[nodiscard]] BLOCXXI_CORE_API auto VerifyEventRecord(
  SignedEventRecord const& record) -> bool;

} // namespace blocxxi::core
