//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <Nova/Base/Compilers.h>

#include <cryptopp/eccrypto.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>

#include <Blocxxi/Crypto/signature.h>

namespace blocxxi::crypto {
namespace {

namespace cr = CryptoPP;
using PrivateKeyType = cr::ECDSA<cr::ECP, cr::SHA256>::PrivateKey;
using PublicKeyType = cr::ECDSA<cr::ECP, cr::SHA256>::PublicKey;

auto MakePrivateKey(KeyPair const& key_pair) -> PrivateKeyType
{
  auto private_key = PrivateKeyType {};
  auto const exponent = cr::Integer(key_pair.Secret().Data(), key_pair.Secret().Size(),
    cr::Integer::UNSIGNED, cr::ByteOrder::BIG_ENDIAN_ORDER);
  private_key.Initialize(cr::ASN1::secp256k1(), exponent);
  return private_key;
}

} // namespace

TEST(SignatureTest, SignAndVerifyRoundTrip)
{
  auto key_pair = KeyPair();
  auto const payload = std::vector<std::uint8_t> { 0x10U, 0x20U, 0x30U, 0x40U };
  auto const private_key = MakePrivateKey(key_pair);
  auto public_key = PublicKeyType {};
  private_key.MakePublicKey(public_key);
  auto direct_signer = cr::ECDSA<cr::ECP, cr::SHA256>::Signer(private_key);
  auto prng = cr::AutoSeededRandomPool {};
  auto direct_signature = std::vector<std::uint8_t>(direct_signer.MaxSignatureLength());
  auto const direct_length
    = direct_signer.SignMessage(prng, payload.data(), payload.size(), direct_signature.data());
  direct_signature.resize(direct_length);
  auto direct_verifier = cr::ECDSA<cr::ECP, cr::SHA256>::Verifier(public_key);
  auto direct_public = KeyPair::PublicKey {};
  public_key.GetPublicElement().x.Encode(direct_public.Data(), 32U);
  public_key.GetPublicElement().y.Encode(direct_public.Data() + 32U, 32U);

  auto const signature_hex = SignMessageHex(key_pair, payload);

  EXPECT_TRUE(direct_verifier.VerifyMessage(
    payload.data(), payload.size(), direct_signature.data(), direct_signature.size()));
  EXPECT_EQ(direct_public.ToHex(), key_pair.Public().ToHex());
  EXPECT_FALSE(signature_hex.empty());
  EXPECT_TRUE(VerifyMessageHex(key_pair.Public(), payload, signature_hex));
}

TEST(SignatureTest, VerifyRejectsTamperedPayload)
{
  auto key_pair = KeyPair();
  auto const payload = std::vector<std::uint8_t> { 0x01U, 0x02U, 0x03U };
  auto tampered = payload;
  tampered.back() = 0xFFU;

  auto const signature_hex = SignMessageHex(key_pair, payload);

  EXPECT_FALSE(VerifyMessageHex(key_pair.Public(), tampered, signature_hex));
}

} // namespace blocxxi::crypto
