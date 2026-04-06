//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <array>
#include <vector>

#include <Nova/Base/Compilers.h>
#include <gtest/gtest.h>

#include <Blocxxi/P2P/kademlia/message.h>

// Disable compiler and linter warnings originating from the unit test framework
// and for which we cannot do anything. Additionally, every TEST or TEST_X macro
// usage must be preceded by a '// NOLINTNEXTLINE'.
NOVA_DIAGNOSTIC_PUSH
#if NOVA_CLANG_VERSION
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

namespace blocxxi::p2p::kademlia {

namespace {

auto MakeHash(std::string_view hex) -> blocxxi::crypto::Hash160
{
  return blocxxi::crypto::Hash160::FromHex(std::string(hex));
}

} // namespace

// NOLINTNEXTLINE
TEST(MessageSerializationTest, HeaderRoundTripPreservesFields)
{
  Header header;
  header.version_ = Header::Version::V1;
  header.type_ = Header::MessageType::FIND_NODE_RESPONSE;
  header.source_id_ = MakeHash("00112233445566778899aabbccddeeff00112233");
  header.random_token_ = MakeHash("ffeeddccbbaa99887766554433221100ffeeddcc");

  Buffer buffer;
  Serialize(header, buffer);

  Header decoded;
  const auto consumed = Deserialize(BufferReader(buffer), decoded);

  ASSERT_EQ(consumed, buffer.size());
  ASSERT_EQ(decoded.version_, header.version_);
  ASSERT_EQ(decoded.type_, header.type_);
  ASSERT_EQ(decoded.source_id_, header.source_id_);
  ASSERT_EQ(decoded.random_token_, header.random_token_);
}

// NOLINTNEXTLINE
TEST(MessageSerializationTest, StoreValueRequestRoundTripPreservesPayloadSize)
{
  StoreValueRequestBody body;
  body.data_key_ = MakeHash("0123456789abcdeffedcba987654321001234567");
  body.data_value_ = {0x00, 0x01, 0x02, 0x80, 0xFF, 0x10};

  Buffer buffer;
  Serialize(body, buffer);

  StoreValueRequestBody decoded;
  const auto consumed = Deserialize(BufferReader(buffer), decoded);

  ASSERT_EQ(consumed, buffer.size());
  ASSERT_EQ(decoded.data_key_, body.data_key_);
  ASSERT_EQ(decoded.data_value_, body.data_value_);
}

// NOLINTNEXTLINE
TEST(MessageSerializationTest, FindNodeResponseRoundTripPreservesPeerPorts)
{
  FindNodeResponseBody body;
  body.peers_.emplace_back(
    MakeHash("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), "127.0.0.1", 6881);
  body.peers_.emplace_back(
    MakeHash("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"), "127.0.0.2", 51413);

  Buffer buffer;
  Serialize(body, buffer);

  FindNodeResponseBody decoded;
  const auto consumed = Deserialize(BufferReader(buffer), decoded);

  ASSERT_EQ(consumed, buffer.size());
  ASSERT_EQ(decoded.peers_.size(), body.peers_.size());
  ASSERT_EQ(decoded.peers_.at(0).Id(), body.peers_.at(0).Id());
  ASSERT_EQ(decoded.peers_.at(0).Endpoint(), body.peers_.at(0).Endpoint());
  ASSERT_EQ(decoded.peers_.at(1).Id(), body.peers_.at(1).Id());
  ASSERT_EQ(decoded.peers_.at(1).Endpoint(), body.peers_.at(1).Endpoint());
}

} // namespace blocxxi::p2p::kademlia
NOVA_DIAGNOSTIC_POP
