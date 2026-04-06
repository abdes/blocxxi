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
#include <filesystem>
#include <sstream>
#include <span>
#include <string>
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

[[nodiscard]] auto WireHashToHex(std::span<std::uint8_t const> bytes) -> std::string
{
  auto reversed = std::vector<std::uint8_t>(bytes.rbegin(), bytes.rend());
  return ToHex(reversed);
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

auto WritePreviousHash(
  std::array<std::uint8_t, 80>& header, std::string const& previous_hash_hex) -> void
{
  auto bytes = std::array<std::uint8_t, 32> {};
  for (auto index = std::size_t { 0 }; index < 32U; ++index) {
    auto const chunk = previous_hash_hex.substr(index * 2U, 2U);
    auto parsed = std::uint32_t { 0 };
    auto stream = std::stringstream {};
    stream << std::hex << chunk;
    stream >> parsed;
    bytes[31U - index] = static_cast<std::uint8_t>(parsed);
  }
  std::copy(bytes.begin(), bytes.end(), header.begin() + 4U);
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
  ASSERT_EQ(result.headers.size(), 2U);
  EXPECT_EQ(result.headers.front().height, 1U);
  EXPECT_EQ(result.headers.back().height, 2U);
  EXPECT_EQ(result.header_hashes.front(), expected1);
  EXPECT_EQ(result.header_hashes.back(), expected2);
  EXPECT_EQ(result.headers.front().hash_hex, expected1);
  EXPECT_EQ(result.headers.back().hash_hex, expected2);
  EXPECT_NE(std::find(result.command_trace.begin(), result.command_trace.end(), "version"),
    result.command_trace.end());
  EXPECT_NE(std::find(result.command_trace.begin(), result.command_trace.end(), "verack"),
    result.command_trace.end());
}

TEST(BitcoinAdapterTest, HeaderSyncAdapterImportsLiveHeadersIntoKernel)
{
  auto io_context = asio::io_context {};
  auto acceptor
    = asio::ip::tcp::acceptor(io_context, { asio::ip::make_address("127.0.0.1"), 0 });
  auto const port = acceptor.local_endpoint().port();

  auto const header1 = MakeHeaderBytes(4U, 21U, 1598918403U);
  auto header2 = MakeHeaderBytes(4U, 22U, 1598918404U);
  auto const expected1 = HeaderHashHex(header1);
  WritePreviousHash(header2, expected1);
  auto const expected2 = HeaderHashHex(header2);

  auto server = std::thread([&]() {
    auto socket = asio::ip::tcp::socket(io_context);
    acceptor.accept(socket);

    auto const [version_command, _version_payload] = ReadMessage(socket);
    EXPECT_EQ(version_command, "version");

    auto version_reply = std::vector<std::uint8_t> {};
    AppendLittleEndian(version_reply, 70016U, 4U);
    asio::write(socket, asio::buffer(EncodeMessage("version", version_reply)));
    asio::write(socket,
      asio::buffer(EncodeMessage("verack", std::span<std::uint8_t const> {})));

    auto const [verack_command, _verack_payload] = ReadMessage(socket);
    EXPECT_EQ(verack_command, "verack");
    auto const [getheaders_command, _getheaders_payload] = ReadMessage(socket);
    EXPECT_EQ(getheaders_command, "getheaders");

    auto headers_payload = std::vector<std::uint8_t> {};
    AppendCompactSize(headers_payload, 2U);
    headers_payload.insert(headers_payload.end(), header1.begin(), header1.end());
    headers_payload.push_back(0U);
    headers_payload.insert(headers_payload.end(), header2.begin(), header2.end());
    headers_payload.push_back(0U);
    asio::write(socket, asio::buffer(EncodeMessage("headers", headers_payload)));
  });

  auto node = node::Node();
  ASSERT_TRUE(node.Start().ok());
  auto adapter = HeaderSyncAdapter({
    .network = Network::Signet,
    .peer_hint = "127.0.0.1",
    .header_sync_only = true,
  });
  ASSERT_TRUE(adapter.Bind(node).ok());

  auto live_result = SignetHeadersResult {};
  auto const status = adapter.ImportLiveSignetHeaders({
      .host = "127.0.0.1",
      .port = port,
    },
    &live_result);

  server.join();

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(adapter.ImportedHeights(),
    (std::vector<std::uint32_t> { 1U, 2U }));
  EXPECT_EQ(node.Snapshot().height, 2U);
  ASSERT_EQ(node.Blocks().size(), 3U);
  EXPECT_NE(node.Blocks()[1].transactions.front().PayloadText().find(expected1),
    std::string::npos);
  EXPECT_EQ(live_result.headers.size(), 2U);
  EXPECT_EQ(live_result.headers.front().hash_hex, expected1);
  EXPECT_EQ(live_result.headers.back().hash_hex, expected2);
}

TEST(BitcoinAdapterTest, HeaderSyncAdapterResumesLiveImportFromPersistedLocator)
{
  auto const state_root
    = std::filesystem::temp_directory_path() / "blocxxi-bitcoin-import-resume";
  std::filesystem::remove_all(state_root);

  auto first_io = asio::io_context {};
  auto first_acceptor = asio::ip::tcp::acceptor(
    first_io, { asio::ip::make_address("127.0.0.1"), 0 });
  auto const first_port = first_acceptor.local_endpoint().port();

  auto const header1 = MakeHeaderBytes(4U, 31U, 1598918405U);
  auto header2 = MakeHeaderBytes(4U, 32U, 1598918406U);
  auto const expected1 = HeaderHashHex(header1);
  WritePreviousHash(header2, expected1);
  auto const expected2 = HeaderHashHex(header2);

  auto first_server = std::thread([&]() {
    auto socket = asio::ip::tcp::socket(first_io);
    first_acceptor.accept(socket);
    (void)ReadMessage(socket); // version
    auto version_reply = std::vector<std::uint8_t> {};
    AppendLittleEndian(version_reply, 70016U, 4U);
    asio::write(socket, asio::buffer(EncodeMessage("version", version_reply)));
    asio::write(socket,
      asio::buffer(EncodeMessage("verack", std::span<std::uint8_t const> {})));
    (void)ReadMessage(socket); // verack
    (void)ReadMessage(socket); // getheaders

    auto headers_payload = std::vector<std::uint8_t> {};
    AppendCompactSize(headers_payload, 2U);
    headers_payload.insert(headers_payload.end(), header1.begin(), header1.end());
    headers_payload.push_back(0U);
    headers_payload.insert(headers_payload.end(), header2.begin(), header2.end());
    headers_payload.push_back(0U);
    asio::write(socket, asio::buffer(EncodeMessage("headers", headers_payload)));
  });

  auto options = node::NodeOptions {};
  options.storage_mode = node::StorageMode::FileSystem;
  options.storage_root = state_root;
  auto node = node::Node(options);
  ASSERT_TRUE(node.Start().ok());
  auto adapter = HeaderSyncAdapter({
    .network = Network::Signet,
    .peer_hint = "127.0.0.1",
    .header_sync_only = true,
  });
  ASSERT_TRUE(adapter.Bind(node).ok());

  auto first_result = SignetHeadersResult {};
  auto status = adapter.ImportLiveSignetHeaders({
      .host = "127.0.0.1",
      .port = first_port,
      .state_root = state_root,
    },
    &first_result);
  first_server.join();

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(node.Snapshot().height, 2U);
  ASSERT_EQ(node.Blocks().size(), 3U);

  auto second_io = asio::io_context {};
  auto second_acceptor = asio::ip::tcp::acceptor(
    second_io, { asio::ip::make_address("127.0.0.1"), 0 });
  auto const second_port = second_acceptor.local_endpoint().port();

  auto header3 = MakeHeaderBytes(4U, 33U, 1598918407U);
  WritePreviousHash(header3, expected2);
  auto const expected3 = HeaderHashHex(header3);
  auto observed_locator = std::string {};

  auto second_server = std::thread([&]() {
    auto socket = asio::ip::tcp::socket(second_io);
    second_acceptor.accept(socket);
    (void)ReadMessage(socket); // version
    auto version_reply = std::vector<std::uint8_t> {};
    AppendLittleEndian(version_reply, 70016U, 4U);
    asio::write(socket, asio::buffer(EncodeMessage("version", version_reply)));
    asio::write(socket,
      asio::buffer(EncodeMessage("verack", std::span<std::uint8_t const> {})));
    (void)ReadMessage(socket); // verack
    auto const [_command, getheaders_payload] = ReadMessage(socket);

    auto offset = std::size_t { 4U };
    auto const count = getheaders_payload[offset++];
    EXPECT_EQ(count, 1U);
    observed_locator = WireHashToHex(std::span<std::uint8_t const>(
      getheaders_payload.data() + offset, 32U));
    offset += 32U;

    auto headers_payload = std::vector<std::uint8_t> {};
    AppendCompactSize(headers_payload, 1U);
    headers_payload.insert(headers_payload.end(), header3.begin(), header3.end());
    headers_payload.push_back(0U);
    asio::write(socket, asio::buffer(EncodeMessage("headers", headers_payload)));
  });

  auto second_result = SignetHeadersResult {};
  status = adapter.ImportLiveSignetHeaders({
      .host = "127.0.0.1",
      .port = second_port,
      .state_root = state_root,
    },
    &second_result);
  second_server.join();

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(observed_locator, expected2);
  EXPECT_EQ(second_result.headers.size(), 1U);
  EXPECT_EQ(second_result.headers.front().height, 3U);
  EXPECT_EQ(second_result.headers.front().hash_hex, expected3);
  EXPECT_EQ(node.Snapshot().height, 3U);
  ASSERT_EQ(node.Blocks().size(), 4U);

  std::filesystem::remove_all(state_root);
}

TEST(BitcoinAdapterTest, SignetLiveClientFetchesBoundedBlockBodiesFromPeer)
{
  auto io_context = asio::io_context {};
  auto acceptor
    = asio::ip::tcp::acceptor(io_context, { asio::ip::make_address("127.0.0.1"), 0 });
  auto const port = acceptor.local_endpoint().port();

  auto const header = MakeHeaderBytes(4U, 41U, 1598918408U);
  auto const expected_hash = HeaderHashHex(header);
  auto block_payload = std::vector<std::uint8_t>(header.begin(), header.end());
  block_payload.push_back(1U); // tx count
  block_payload.push_back(1U); // tx version
  block_payload.push_back(0U);
  block_payload.push_back(0U);
  block_payload.push_back(0U);
  block_payload.push_back(0U);
  block_payload.push_back(0U); // vin count
  block_payload.push_back(0U); // vout count
  block_payload.push_back(0U); // locktime
  block_payload.push_back(0U);
  block_payload.push_back(0U);
  block_payload.push_back(0U);
  auto const tx_payload = std::span<std::uint8_t const>(block_payload.data() + 81U, 10U);
  auto const tx_hash = DoubleSha256(tx_payload);
  auto tx_hash_reversed = std::array<std::uint8_t, 32> {};
  std::reverse_copy(tx_hash.begin(), tx_hash.end(), tx_hash_reversed.begin());
  auto const expected_txid = ToHex(tx_hash_reversed);

  auto server = std::thread([&]() {
    auto socket = asio::ip::tcp::socket(io_context);
    acceptor.accept(socket);
    (void)ReadMessage(socket); // version

    auto version_reply = std::vector<std::uint8_t> {};
    AppendLittleEndian(version_reply, 70016U, 4U);
    asio::write(socket, asio::buffer(EncodeMessage("version", version_reply)));
    asio::write(socket,
      asio::buffer(EncodeMessage("verack", std::span<std::uint8_t const> {})));

    (void)ReadMessage(socket); // verack
    auto const [getdata_command, getdata_payload] = ReadMessage(socket);
    EXPECT_EQ(getdata_command, "getdata");
    EXPECT_FALSE(getdata_payload.empty());

    asio::write(socket, asio::buffer(EncodeMessage("block", block_payload)));
  });

  auto client = SignetLiveClient({
    .host = "127.0.0.1",
    .port = port,
  });
  auto result = SignetBlocksResult {};
  auto const status = client.FetchBlocks(
    std::span<std::string const>(&expected_hash, 1U), result);

  server.join();

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(result.peer_address, "127.0.0.1");
  EXPECT_EQ(result.protocol_version, 70016);
  ASSERT_EQ(result.blocks.size(), 1U);
  EXPECT_EQ(result.blocks.front().block_hash_hex, expected_hash);
  EXPECT_GT(result.blocks.front().payload.size(), 80U);
  ASSERT_EQ(result.metadata.size(), 1U);
  EXPECT_EQ(result.metadata.front().block_hash_hex, expected_hash);
  EXPECT_EQ(result.metadata.front().version, 4U);
  EXPECT_EQ(result.metadata.front().nonce, 41U);
  EXPECT_EQ(result.metadata.front().transaction_count, 1U);
  ASSERT_EQ(result.metadata.front().transaction_sizes.size(), 1U);
  EXPECT_EQ(result.metadata.front().transaction_sizes.front(), 10U);
  ASSERT_EQ(result.metadata.front().transaction_versions.size(), 1U);
  EXPECT_EQ(result.metadata.front().transaction_versions.front(), 1U);
  ASSERT_EQ(result.metadata.front().transaction_input_counts.size(), 1U);
  ASSERT_EQ(result.metadata.front().transaction_output_counts.size(), 1U);
  EXPECT_EQ(result.metadata.front().transaction_input_counts.front(), 0U);
  EXPECT_EQ(result.metadata.front().transaction_output_counts.front(), 0U);
  ASSERT_EQ(result.metadata.front().transaction_has_witness.size(), 1U);
  EXPECT_FALSE(result.metadata.front().transaction_has_witness.front());
  ASSERT_EQ(result.metadata.front().transaction_ids.size(), 1U);
  ASSERT_EQ(result.metadata.front().transaction_witness_ids.size(), 1U);
  EXPECT_EQ(result.metadata.front().transaction_ids.front(), expected_txid);
  EXPECT_EQ(result.metadata.front().transaction_witness_ids.front(), expected_txid);
  EXPECT_NE(std::find(result.command_trace.begin(), result.command_trace.end(), "block"),
    result.command_trace.end());
}

TEST(BitcoinAdapterTest, WitnessTransactionsProduceDistinctTxidAndWtxid)
{
  auto io_context = asio::io_context {};
  auto acceptor
    = asio::ip::tcp::acceptor(io_context, { asio::ip::make_address("127.0.0.1"), 0 });
  auto const port = acceptor.local_endpoint().port();

  auto const header = MakeHeaderBytes(4U, 51U, 1598918409U);
  auto block_payload = std::vector<std::uint8_t>(header.begin(), header.end());
  block_payload.push_back(1U); // tx count

  auto tx = std::vector<std::uint8_t> {};
  tx.push_back(1U); // version little-endian
  tx.push_back(0U);
  tx.push_back(0U);
  tx.push_back(0U);
  tx.push_back(0U); // marker
  tx.push_back(1U); // flags
  tx.push_back(1U); // vin count
  tx.insert(tx.end(), 32U, 0U); // prevout hash
  tx.insert(tx.end(), { 0xFF, 0xFF, 0xFF, 0xFF }); // prevout index
  tx.push_back(0U); // scriptSig size
  tx.insert(tx.end(), { 0xFF, 0xFF, 0xFF, 0xFF }); // sequence
  tx.push_back(1U); // vout count
  tx.insert(tx.end(), { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }); // value
  tx.push_back(0U); // scriptPubKey size
  tx.push_back(1U); // witness stack items
  tx.push_back(1U); // witness item size
  tx.push_back(0x42); // witness item data
  tx.insert(tx.end(), { 0x00, 0x00, 0x00, 0x00 }); // locktime

  block_payload.insert(block_payload.end(), tx.begin(), tx.end());

  auto const wtxid_hash = DoubleSha256(tx);
  auto wtxid_reversed = std::array<std::uint8_t, 32> {};
  std::reverse_copy(wtxid_hash.begin(), wtxid_hash.end(), wtxid_reversed.begin());
  auto const expected_wtxid = ToHex(wtxid_reversed);

  auto server = std::thread([&]() {
    auto socket = asio::ip::tcp::socket(io_context);
    acceptor.accept(socket);
    (void)ReadMessage(socket); // version

    auto version_reply = std::vector<std::uint8_t> {};
    AppendLittleEndian(version_reply, 70016U, 4U);
    asio::write(socket, asio::buffer(EncodeMessage("version", version_reply)));
    asio::write(socket,
      asio::buffer(EncodeMessage("verack", std::span<std::uint8_t const> {})));

    (void)ReadMessage(socket); // verack
    (void)ReadMessage(socket); // getdata
    asio::write(socket, asio::buffer(EncodeMessage("block", block_payload)));
  });

  auto client = SignetLiveClient({
    .host = "127.0.0.1",
    .port = port,
  });
  auto result = SignetBlocksResult {};
  auto const expected_block_hash = HeaderHashHex(header);
  auto const status = client.FetchBlocks(
    std::span<std::string const>(&expected_block_hash, 1U), result);

  server.join();

  ASSERT_TRUE(status.ok());
  ASSERT_EQ(result.metadata.size(), 1U);
  ASSERT_EQ(result.metadata.front().transaction_ids.size(), 1U);
  ASSERT_EQ(result.metadata.front().transaction_witness_ids.size(), 1U);
  ASSERT_EQ(result.metadata.front().transaction_has_witness.size(), 1U);
  EXPECT_TRUE(result.metadata.front().transaction_has_witness.front());
  EXPECT_EQ(result.metadata.front().transaction_witness_ids.front(), expected_wtxid);
  EXPECT_NE(result.metadata.front().transaction_ids.front(),
    result.metadata.front().transaction_witness_ids.front());
}

} // namespace blocxxi::bitcoin
