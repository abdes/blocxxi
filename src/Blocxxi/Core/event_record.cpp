//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/Core/event_record.h>

#include <algorithm>
#include <string_view>

#include <Blocxxi/Crypto/signature.h>

namespace blocxxi::core {
namespace {

constexpr auto kHexDigits = std::string_view { "0123456789abcdef" };

auto EncodeHex(ByteVector const& payload) -> std::string
{
  auto text = std::string {};
  text.reserve(payload.size() * 2U);
  for (auto const byte : payload) {
    text.push_back(kHexDigits[(byte >> 4U) & 0x0FU]);
    text.push_back(kHexDigits[byte & 0x0FU]);
  }
  return text;
}

auto SortedIdentifiers(std::vector<std::string> values) -> std::vector<std::string>
{
  std::sort(values.begin(), values.end());
  return values;
}

auto SortedAttributes(std::vector<EventAttribute> values) -> std::vector<EventAttribute>
{
  std::sort(values.begin(), values.end(),
    [](EventAttribute const& lhs, EventAttribute const& rhs) {
      return lhs.key == rhs.key ? lhs.value < rhs.value : lhs.key < rhs.key;
    });
  return values;
}

auto CanonicalSeed(EventEnvelope const& envelope) -> std::string
{
  auto output = std::string {};
  output.reserve(256U + envelope.summary.size() + envelope.payload.size() * 2U);

  output += "schema=" + envelope.schema + '\n';
  output += "event_type=" + envelope.event_type + '\n';
  output += "taxonomy=" + envelope.taxonomy + '\n';
  output += "source=" + envelope.source + '\n';
  output += "producer=" + envelope.producer + '\n';
  output += "window_start=" + std::to_string(envelope.window.start_utc) + '\n';
  output += "window_end=" + std::to_string(envelope.window.end_utc) + '\n';
  output += "observed_at=" + std::to_string(envelope.observed_at_utc) + '\n';
  output += "published_at=" + std::to_string(envelope.published_at_utc) + '\n';

  output += "identifiers=";
  auto const identifiers = SortedIdentifiers(envelope.identifiers);
  for (auto index = std::size_t { 0 }; index < identifiers.size(); ++index) {
    if (index > 0U) {
      output.push_back(',');
    }
    output += identifiers[index];
  }
  output.push_back('\n');

  output += "attributes=";
  auto const attributes = SortedAttributes(envelope.attributes);
  for (auto index = std::size_t { 0 }; index < attributes.size(); ++index) {
    if (index > 0U) {
      output.push_back('|');
    }
    output += attributes[index].key;
    output.push_back('=');
    output += attributes[index].value;
  }
  output.push_back('\n');

  output += "summary=" + envelope.summary + '\n';
  output += "payload_hex=" + EncodeHex(envelope.payload) + '\n';
  return output;
}

} // namespace

auto EventEnvelope::CanonicalText() const -> std::string
{
  return CanonicalSeed(*this);
}

auto EventEnvelope::CanonicalBytes() const -> ByteVector
{
  auto const text = CanonicalText();
  return ByteVector(text.begin(), text.end());
}

auto EventEnvelope::ContentId() const -> blocxxi::crypto::Hash256
{
  return MakeId(CanonicalText());
}

auto SignedEventRecord::DeterministicKey() const -> std::string
{
  return DeriveDeterministicEventKey(envelope);
}

auto DeriveDeterministicEventKey(EventEnvelope const& envelope) -> std::string
{
  auto seed = std::string {};
  seed += envelope.schema;
  seed.push_back('|');
  seed += envelope.event_type;
  seed.push_back('|');
  seed += envelope.taxonomy;
  seed.push_back('|');
  seed += std::to_string(envelope.window.start_utc);
  seed.push_back('|');
  seed += std::to_string(envelope.window.end_utc);

  auto const identifiers = SortedIdentifiers(envelope.identifiers);
  for (auto const& identifier : identifiers) {
    seed.push_back('|');
    seed += identifier;
  }

  seed.push_back('|');
  seed += envelope.ContentId().ToHex();
  return MakeId(seed).ToHex();
}

auto SignEventRecord(EventEnvelope envelope, blocxxi::crypto::KeyPair const& key_pair,
  std::string signer_name) -> SignedEventRecord
{
  if (envelope.observed_at_utc == 0) {
    envelope.observed_at_utc = NowUnixSeconds();
  }
  if (envelope.published_at_utc == 0) {
    envelope.published_at_utc = envelope.observed_at_utc;
  }

  auto const canonical = envelope.CanonicalBytes();
  return SignedEventRecord {
    .envelope = std::move(envelope),
    .signer_name = std::move(signer_name),
    .signer_public_key_hex = blocxxi::crypto::PublicKeyToHex(key_pair.Public()),
    .signature_hex = blocxxi::crypto::SignMessageHex(key_pair, canonical),
  };
}

auto VerifyEventRecord(SignedEventRecord const& record) -> bool
{
  auto const public_key = blocxxi::crypto::PublicKeyFromHex(record.signer_public_key_hex);
  if (!public_key.has_value()) {
    return false;
  }

  auto const canonical = record.envelope.CanonicalBytes();
  return blocxxi::crypto::VerifyMessageHex(
    *public_key, canonical, record.signature_hex);
}

} // namespace blocxxi::core
