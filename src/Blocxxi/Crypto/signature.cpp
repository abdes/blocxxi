//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/Crypto/signature.h>

#include <stdexcept>

#include <Nova/Base/Compilers.h>

NOVA_DIAGNOSTIC_PUSH
#if NOVA_GNUC_VERSION
#pragma GCC diagnostic ignored "-Wshadow"
#endif
#include <cryptopp/eccrypto.h>
#include <cryptopp/files.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include <cryptopp/sha.h>
#include <cryptopp/queue.h>
NOVA_DIAGNOSTIC_POP

#include <Blocxxi/Codec/base16.h>

namespace blocxxi::crypto {
namespace {

namespace cr = CryptoPP;

using PrivateKeyType = cr::ECDSA<cr::ECP, cr::SHA256>::PrivateKey;
using PublicKeyType = cr::ECDSA<cr::ECP, cr::SHA256>::PublicKey;

auto MakePrivateKey(KeyPair const& key_pair) -> PrivateKeyType
{
  auto private_key = PrivateKeyType {};
  auto const private_exponent = cr::Integer(key_pair.Secret().Data(),
    KeyPair::PrivateKey::Size(), cr::Integer::UNSIGNED,
    cr::ByteOrder::BIG_ENDIAN_ORDER);
  private_key.Initialize(cr::ASN1::secp256k1(), private_exponent);
  return private_key;
}

auto MakePublicKey(KeyPair::PublicKey const& public_key_hash) -> PublicKeyType
{
  auto public_key = PublicKeyType {};
  public_key.AccessGroupParameters().Initialize(cr::ASN1::secp256k1());
  auto encoded = std::array<std::uint8_t, 65> {};
  encoded.front() = 0x04U;
  std::copy_n(public_key_hash.Data(), 64U, encoded.begin() + 1U);
  auto point = cr::ECP::Point {};
  if (!public_key.GetGroupParameters().GetCurve().DecodePoint(
        point, encoded.data(), encoded.size())) {
    throw std::invalid_argument("invalid secp256k1 public point encoding");
  }
  public_key.Initialize(cr::ASN1::secp256k1(), point);
  auto prng = cr::AutoSeededRandomPool {};
  if (!public_key.Validate(prng, 3)) {
    throw std::invalid_argument("invalid secp256k1 public key");
  }
  return public_key;
}

auto DecodeHex(std::string const& value) -> SignatureBytes
{
  if (value.empty()) {
    return {};
  }
  if ((value.size() % 2U) != 0U) {
    throw std::invalid_argument("signature hex must have an even number of digits");
  }

  auto bytes = SignatureBytes(value.size() / 2U);
  codec::hex::Decode(value, std::span<std::uint8_t>(bytes));
  return bytes;
}

} // namespace

auto PublicKeyToHex(KeyPair::PublicKey const& public_key) -> std::string
{
  return public_key.ToHex();
}

auto PublicKeyFromHex(std::string const& public_key_hex)
  -> std::optional<KeyPair::PublicKey>
{
  if (public_key_hex.size() != KeyPair::PublicKey::Size() * 2U) {
    return std::nullopt;
  }

  try {
    return KeyPair::PublicKey::FromHex(public_key_hex, false);
  } catch (std::exception const&) {
    return std::nullopt;
  }
}

auto SignMessage(KeyPair const& key_pair, std::span<std::uint8_t const> message)
  -> SignatureBytes
{
  auto signer = cr::ECDSA<cr::ECP, cr::SHA256>::Signer(MakePrivateKey(key_pair));
  auto prng = cr::AutoSeededRandomPool {};
  auto signature = SignatureBytes(signer.MaxSignatureLength());
  auto const length = signer.SignMessage(
    prng, message.data(), message.size(), signature.data());
  signature.resize(length);
  return signature;
}

auto SignMessageHex(KeyPair const& key_pair, std::span<std::uint8_t const> message)
  -> std::string
{
  auto const signature = SignMessage(key_pair, message);
  return codec::hex::Encode(std::span<std::uint8_t const>(signature), false, true);
}

auto VerifyMessage(KeyPair::PublicKey const& public_key,
  std::span<std::uint8_t const> message, std::span<std::uint8_t const> signature)
  -> bool
{
  try {
    auto verifier = cr::ECDSA<cr::ECP, cr::SHA256>::Verifier(MakePublicKey(public_key));
    return verifier.VerifyMessage(
      message.data(), message.size(), signature.data(), signature.size());
  } catch (std::exception const&) {
    return false;
  }
}

auto VerifyMessageHex(KeyPair::PublicKey const& public_key,
  std::span<std::uint8_t const> message, std::string const& signature_hex) -> bool
{
  try {
    auto const signature = DecodeHex(signature_hex);
    return VerifyMessage(public_key, message, signature);
  } catch (std::exception const&) {
    return false;
  }
}

} // namespace blocxxi::crypto
