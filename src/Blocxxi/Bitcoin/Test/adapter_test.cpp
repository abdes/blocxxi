//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <thread>
#include <vector>

#include <Nova/Base/Sha256.h>

#include <asio.hpp>

#include <Blocxxi/Bitcoin/adapter.h>

namespace blocxxi::bitcoin {
namespace {

constexpr auto kSignetMagic = std::array<std::uint8_t, 4> { 0x0a, 0x03, 0xcf, 0x40 };

[[nodiscard]] auto DoubleSha256(std::span<std::uint8_t const> payload)
  -> std::array<std::uint8_t, 32>
{
  auto const bytes
    = std::span<std::byte const>(reinterpret_cast<std::byte const*>(payload.data()), payload.size());
  auto first = nova::ComputeSha256(bytes);
  auto second = nova::ComputeSha256(
    std::span<std::byte const>(reinterpret_cast<std::byte const*>(first.data()), first.size()));
  return second;
}

[[nodiscard]] auto ToHex(std::span<std::uint8_t const> bytes) -> std::string
{
  constexpr auto kHexDigits = std::string_view { "0123456789abcdef" };
  auto encoded = std::string {};
  encoded.reserve(bytes.size() * 2U);
  for (auto const byte : bytes) {
    encoded.push_back(kHexDigits[(byte >> 4U) & 0x0FU]);
    encoded.push_back(kHexDigits[byte & 0x0FU]);
  }
  return encoded;
}

auto AppendLittleEndian(
  std::vector<std::uint8_t>& out, std::uint64_t value, std::size_t width) -> void
{
  for (auto index = std::size_t { 0 }; index < width; ++index) {
    out.push_back(static_cast<std::uint8_t>((value >> (index * 8U)) & 0xFFU));
  }
}

auto AppendCompactSize(std::vector<std::uint8_t>& out, std::uint64_t value)
  -> void
{
  if (value < 253U) {
    out.push_back(static_cast<std::uint8_t>(value));
    return;
  }
  out.push_back(0xFD);
  AppendLittleEndian(out, value, 2U);
}

[[nodiscard]] auto EncodeMessage(
  std::string_view command, std::span<std::uint8_t const> payload) -> std::vector<std::uint8_t>
{
  auto message = std::vector<std::uint8_t> {};
  message.insert(message.end(), kSignetMagic.begin(), kSignetMagic.end());
  auto padded_command = std::array<std::uint8_t, 12> {};
  std::copy(command.begin(), command.end(), padded_command.begin());
  message.insert(message.end(), padded_command.begin(), padded_command.end());
  AppendLittleEndian(message, payload.size(), 4U);
  auto const checksum = DoubleSha256(payload);
  message.insert(message.end(), checksum.begin(), checksum.begin() + 4U);
  message.insert(message.end(), payload.begin(), payload.end());
  return message;
}

auto ReadExact(asio::ip::tcp::socket& socket, std::size_t size)
  -> std::vector<std::uint8_t>
{
  auto buffer = std::vector<std::uint8_t>(size);
  asio::read(socket, asio::buffer(buffer));
  return buffer;
}

auto ReadMessage(asio::ip::tcp::socket& socket)
  -> std::pair<std::string, std::vector<std::uint8_t>>
{
  auto const header = ReadExact(socket, 24U);
  auto command = std::string {};
  for (auto index = std::size_t { 4U }; index < 16U && header[index] != 0U; ++index) {
    command.push_back(static_cast<char>(header[index]));
  }

  auto payload_size = std::uint32_t { 0 };
  std::memcpy(&payload_size, header.data() + 16U, sizeof(payload_size));
  return { std::move(command), ReadExact(socket, payload_size) };
}

[[nodiscard]] auto MakeHeaderBytes(
  std::uint32_t version, std::uint32_t nonce, std::uint32_t timestamp) -> std::array<std::uint8_t, 80>
{
  auto header = std::array<std::uint8_t, 80> {};
  std::memcpy(header.data(), &version, sizeof(version));
  std::memcpy(header.data() + 68U, &timestamp, sizeof(timestamp));
  auto const bits = std::uint32_t { 0x1e0377aeU };
  std::memcpy(header.data() + 72U, &bits, sizeof(bits));
  std::memcpy(header.data() + 76U, &nonce, sizeof(nonce));
  return header;
}

[[nodiscard]] auto HeaderHashHex(std::span<std::uint8_t const> header) -> std::string
{
  auto const hash = DoubleSha256(header);
  auto reversed = std::array<std::uint8_t, 32> {};
  std::reverse_copy(hash.begin(), hash.end(), reversed.begin());
  return ToHex(reversed);
}

} // namespace

TEST(BitcoinAdapterTest, HeaderSyncAdapterUsesPublicNodeApi)
{
  auto node = node::Node();
  auto events = std::vector<blocxxi::core::ChainEvent> {};
  node.Subscribe([&](blocxxi::core::ChainEvent const& event) {
    events.push_back(event);
  });
  ASSERT_TRUE(node.Start().ok());

  auto adapter = HeaderSyncAdapter({
    .network = Network::Signet,
    .peer_hint = "seed.signet.example",
    .header_sync_only = true,
  });
  ASSERT_TRUE(adapter.Bind(node).ok());

  auto const status = adapter.SubmitHeader({
    .height = 1,
    .hash_hex = "0001",
    .previous_hash_hex = "0000",
    .version = 0x20000000,
  });

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(adapter.ImportedHeights().size(), 1U);
  EXPECT_EQ(node.Blocks().size(), 2U);
  EXPECT_EQ(node.Snapshot().height, 1);
  ASSERT_GE(events.size(), 4U);
  EXPECT_EQ(events[2].type, blocxxi::core::EventType::DiscoveryAttached);
  EXPECT_EQ(events[2].message, "bitcoin.peer:seed.signet.example");
  EXPECT_EQ(events[3].type, blocxxi::core::EventType::AdapterAttached);
  EXPECT_EQ(events[3].message, "bitcoin.header-sync:signet:headers-only");
  EXPECT_EQ(node.Blocks().back().transactions.front().metadata,
    "network=signet;mode=headers-only;peer_hint=seed.signet.example");
}

TEST(BitcoinAdapterTest, HeaderSyncAdapterImportsOrderedHeaderBatch)
{
  auto node = node::Node();
  ASSERT_TRUE(node.Start().ok());

  auto adapter = HeaderSyncAdapter({
    .network = Network::Signet,
    .peer_hint = "seed.signet.example",
    .header_sync_only = true,
  });
  ASSERT_TRUE(adapter.Bind(node).ok());

  auto const headers = std::array {
    Header {
      .height = 1,
      .hash_hex = "0001",
      .previous_hash_hex = "0000",
      .version = 0x20000000,
    },
    Header {
      .height = 2,
      .hash_hex = "0002",
      .previous_hash_hex = "0001",
      .version = 0x20000000,
    },
    Header {
      .height = 3,
      .hash_hex = "0003",
      .previous_hash_hex = "0002",
      .version = 0x20000000,
    },
  };
  auto const status = adapter.SubmitHeaders(headers);

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(adapter.ImportedHeights(),
    (std::vector<std::uint32_t> { 1U, 2U, 3U }));
  EXPECT_EQ(node.Snapshot().height, 3);
  ASSERT_EQ(node.Blocks().size(), 4U);
  EXPECT_EQ(node.Blocks().back().transactions.front().metadata,
    "network=signet;mode=headers-only;peer_hint=seed.signet.example");
}

TEST(BitcoinAdapterTest, HeaderSyncAdapterRejectsUnboundOrIncompleteHeaders)
{
  auto adapter = HeaderSyncAdapter({
    .network = Network::Regtest,
    .header_sync_only = true,
  });

  auto status = adapter.SubmitHeader({
    .height = 1,
    .hash_hex = "01",
    .previous_hash_hex = "00",
    .version = 1,
  });
  EXPECT_EQ(status.code, blocxxi::core::StatusCode::Rejected);

  auto node = node::Node();
  ASSERT_TRUE(node.Start().ok());
  ASSERT_TRUE(adapter.Bind(node).ok());

  status = adapter.SubmitHeader({
    .height = 1,
    .hash_hex = "",
    .previous_hash_hex = "00",
    .version = 1,
  });
  EXPECT_EQ(status.code, blocxxi::core::StatusCode::InvalidArgument);

  status = adapter.SubmitHeader({
    .height = 2,
    .hash_hex = "02",
    .previous_hash_hex = "",
    .version = 1,
  });
  EXPECT_EQ(status.code, blocxxi::core::StatusCode::InvalidArgument);
}

TEST(BitcoinAdapterTest, HeaderSyncAdapterRejectsBrokenHeaderBatch)
{
  auto node = node::Node();
  ASSERT_TRUE(node.Start().ok());

  auto adapter = HeaderSyncAdapter({
    .network = Network::Regtest,
    .header_sync_only = true,
  });
  ASSERT_TRUE(adapter.Bind(node).ok());

  auto const headers = std::array {
    Header {
      .height = 1,
      .hash_hex = "0001",
      .previous_hash_hex = "0000",
      .version = 1,
    },
    Header {
      .height = 3,
      .hash_hex = "0003",
      .previous_hash_hex = "0001",
      .version = 1,
    },
  };
  auto status = adapter.SubmitHeaders(headers);
  EXPECT_EQ(status.code, blocxxi::core::StatusCode::Rejected);
  EXPECT_TRUE(adapter.ImportedHeights().empty());
  EXPECT_EQ(node.Blocks().size(), 1U);
}

TEST(BitcoinAdapterTest, SignetLiveClientFetchesHeadersFromBoundedPeer)
{
  auto io_context = asio::io_context {};
  auto acceptor
    = asio::ip::tcp::acceptor(io_context, { asio::ip::make_address("127.0.0.1"), 0 });
  auto const port = acceptor.local_endpoint().port();

  auto const header1 = MakeHeaderBytes(4U, 11U, 1598918401U);
  auto const header2 = MakeHeaderBytes(4U, 12U, 1598918402U);
  auto const expected1 = HeaderHashHex(header1);
  auto const expected2 = HeaderHashHex(header2);

  auto server = std::thread([&]() {
    auto socket = asio::ip::tcp::socket(io_context);
    acceptor.accept(socket);

    auto const [version_command, version_payload] = ReadMessage(socket);
    EXPECT_EQ(version_command, "version");
    EXPECT_FALSE(version_payload.empty());

    auto version_reply = std::vector<std::uint8_t> {};
    AppendLittleEndian(version_reply, 70016U, 4U);
    asio::write(socket, asio::buffer(EncodeMessage("version", version_reply)));
    asio::write(socket,
      asio::buffer(EncodeMessage("verack", std::span<std::uint8_t const> {})));

    auto const [verack_command, _verack_payload] = ReadMessage(socket);
    EXPECT_EQ(verack_command, "verack");
    auto const [getheaders_command, getheaders_payload] = ReadMessage(socket);
    EXPECT_EQ(getheaders_command, "getheaders");
    EXPECT_FALSE(getheaders_payload.empty());

    auto headers_payload = std::vector<std::uint8_t> {};
    AppendCompactSize(headers_payload, 2U);
    headers_payload.insert(headers_payload.end(), header1.begin(), header1.end());
    headers_payload.push_back(0U);
    headers_payload.insert(headers_payload.end(), header2.begin(), header2.end());
    headers_payload.push_back(0U);
    asio::write(socket, asio::buffer(EncodeMessage("headers", headers_payload)));
  });

  auto client = SignetLiveClient({
    .host = "127.0.0.1",
    .port = port,
  });
  auto result = SignetHeadersResult {};
  auto const status = client.FetchHeaders(result);

  server.join();

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(result.protocol_version, 70016);
  EXPECT_EQ(result.peer_address, "127.0.0.1");
  EXPECT_EQ(result.header_hashes.size(), 2U);
  EXPECT_EQ(result.header_hashes.front(), expected1);
  EXPECT_EQ(result.header_hashes.back(), expected2);
  EXPECT_NE(std::find(result.command_trace.begin(), result.command_trace.end(), "version"),
    result.command_trace.end());
  EXPECT_NE(std::find(result.command_trace.begin(), result.command_trace.end(), "verack"),
    result.command_trace.end());
}

} // namespace blocxxi::bitcoin
