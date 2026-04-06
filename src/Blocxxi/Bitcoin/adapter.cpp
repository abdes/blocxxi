//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/Bitcoin/adapter.h>

#include <Nova/Base/Sha256.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <optional>
#include <sstream>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <asio.hpp>

#include <Blocxxi/Core/primitives.h>

namespace blocxxi::bitcoin {
namespace {

constexpr auto kProtocolVersion = std::int32_t { 70016 };
constexpr auto kSignetMagic = std::array<std::uint8_t, 4> { 0x0a, 0x03, 0xcf, 0x40 };

using TcpSocket = asio::ip::tcp::socket;

[[nodiscard]] auto ToBytes(std::string_view text) -> std::vector<std::uint8_t>
{
  return std::vector<std::uint8_t>(text.begin(), text.end());
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
  if (value <= 0xFFFFU) {
    out.push_back(0xFD);
    AppendLittleEndian(out, value, 2U);
    return;
  }
  if (value <= 0xFFFFFFFFU) {
    out.push_back(0xFE);
    AppendLittleEndian(out, value, 4U);
    return;
  }
  out.push_back(0xFF);
  AppendLittleEndian(out, value, 8U);
}

[[nodiscard]] auto ReadCompactSize(
  std::span<std::uint8_t const> payload, std::size_t& offset) -> std::optional<std::uint64_t>
{
  if (offset >= payload.size()) {
    return std::nullopt;
  }

  auto const first = payload[offset++];
  if (first < 253U) {
    return first;
  }

  auto const width = first == 0xFD ? 2U : first == 0xFE ? 4U : 8U;
  if (offset + width > payload.size()) {
    return std::nullopt;
  }

  auto value = std::uint64_t { 0 };
  for (auto index = std::size_t { 0 }; index < width; ++index) {
    value |= static_cast<std::uint64_t>(payload[offset + index]) << (index * 8U);
  }
  offset += width;
  return value;
}

[[nodiscard]] auto WireHashFromHex(std::string const& hash_hex)
  -> std::optional<std::array<std::uint8_t, 32>>
{
  if (hash_hex.size() != 64U) {
    return std::nullopt;
  }

  auto bytes = std::array<std::uint8_t, 32> {};
  for (auto index = std::size_t { 0 }; index < 32U; ++index) {
    auto const chunk = hash_hex.substr(index * 2U, 2U);
    auto parsed = std::uint32_t { 0 };
    auto stream = std::stringstream {};
    stream << std::hex << chunk;
    stream >> parsed;
    if (stream.fail()) {
      return std::nullopt;
    }
    bytes[31U - index] = static_cast<std::uint8_t>(parsed);
  }
  return bytes;
}

[[nodiscard]] auto EncodeMessage(
  std::string_view command, std::span<std::uint8_t const> payload) -> std::vector<std::uint8_t>
{
  auto message = std::vector<std::uint8_t> {};
  message.insert(message.end(), kSignetMagic.begin(), kSignetMagic.end());

  auto padded_command = std::array<std::uint8_t, 12> {};
  auto const command_bytes = ToBytes(command);
  std::copy(command_bytes.begin(), command_bytes.end(), padded_command.begin());
  message.insert(message.end(), padded_command.begin(), padded_command.end());

  AppendLittleEndian(message, payload.size(), 4U);
  auto const checksum = DoubleSha256(payload);
  message.insert(message.end(), checksum.begin(), checksum.begin() + 4U);
  message.insert(message.end(), payload.begin(), payload.end());
  return message;
}

[[nodiscard]] auto EncodeVersionPayload(std::string const& remote_address,
  std::uint16_t remote_port) -> std::vector<std::uint8_t>
{
  auto payload = std::vector<std::uint8_t> {};
  auto const now = static_cast<std::uint64_t>(std::time(nullptr));
  auto const user_agent = std::string_view { "/blocxxi:0.1.0/" };

  AppendLittleEndian(payload, static_cast<std::uint32_t>(kProtocolVersion), 4U);
  AppendLittleEndian(payload, 0U, 8U);
  AppendLittleEndian(payload, now, 8U);

  auto append_network_address =
    [&](std::string const& host, std::uint16_t port) {
      AppendLittleEndian(payload, 0U, 8U);
      auto const address = asio::ip::make_address(host);
      if (address.is_v4()) {
        payload.insert(payload.end(), 10U, 0U);
        payload.push_back(0xFF);
        payload.push_back(0xFF);
        auto const bytes = address.to_v4().to_bytes();
        payload.insert(payload.end(), bytes.begin(), bytes.end());
      } else {
        auto const bytes = address.to_v6().to_bytes();
        payload.insert(payload.end(), bytes.begin(), bytes.end());
      }
      payload.push_back(static_cast<std::uint8_t>((port >> 8U) & 0xFFU));
      payload.push_back(static_cast<std::uint8_t>(port & 0xFFU));
    };

  append_network_address(remote_address, remote_port);
  append_network_address("0.0.0.0", 0U);
  AppendLittleEndian(payload, 0x12345678ABCDEF00ULL, 8U);
  AppendCompactSize(payload, user_agent.size());
  payload.insert(payload.end(), user_agent.begin(), user_agent.end());
  AppendLittleEndian(payload, 0U, 4U);
  payload.push_back(0U);
  return payload;
}

[[nodiscard]] auto EncodeGetHeadersPayload(std::string const& locator_hash_hex)
  -> std::optional<std::vector<std::uint8_t>>
{
  auto locator = WireHashFromHex(locator_hash_hex);
  if (!locator.has_value()) {
    return std::nullopt;
  }

  auto payload = std::vector<std::uint8_t> {};
  AppendLittleEndian(payload, static_cast<std::uint32_t>(kProtocolVersion), 4U);
  AppendCompactSize(payload, 1U);
  payload.insert(payload.end(), locator->begin(), locator->end());
  payload.insert(payload.end(), 32U, 0U);
  return payload;
}

[[nodiscard]] auto EncodeGetDataPayload(std::span<std::string const> block_hashes)
  -> std::optional<std::vector<std::uint8_t>>
{
  auto payload = std::vector<std::uint8_t> {};
  AppendCompactSize(payload, block_hashes.size());
  for (auto const& hash_hex : block_hashes) {
    auto hash = WireHashFromHex(hash_hex);
    if (!hash.has_value()) {
      return std::nullopt;
    }
    AppendLittleEndian(payload, 2U, 4U); // MSG_BLOCK
    payload.insert(payload.end(), hash->begin(), hash->end());
  }
  return payload;
}

auto ReadExact(TcpSocket& socket, std::size_t size) -> std::optional<std::vector<std::uint8_t>>
{
  auto buffer = std::vector<std::uint8_t>(size);
  auto offset = std::size_t { 0 };
  while (offset < size) {
    auto error = std::error_code {};
    auto const transferred = socket.read_some(
      asio::buffer(buffer.data() + offset, size - offset), error);
    if (error) {
      return std::nullopt;
    }
    offset += transferred;
  }
  return buffer;
}

struct DecodedMessage {
  std::string command {};
  std::vector<std::uint8_t> payload {};
};

auto ReadMessage(TcpSocket& socket) -> std::optional<DecodedMessage>
{
  auto header = ReadExact(socket, 24U);
  if (!header.has_value()) {
    return std::nullopt;
  }
  if (!std::equal(kSignetMagic.begin(), kSignetMagic.end(), header->begin())) {
    return std::nullopt;
  }

  auto command = std::string {};
  for (auto index = std::size_t { 4U }; index < 16U && (*header)[index] != 0U; ++index) {
    command.push_back(static_cast<char>((*header)[index]));
  }

  auto payload_size = std::uint32_t { 0 };
  std::memcpy(&payload_size, header->data() + 16U, sizeof(payload_size));
  auto payload = ReadExact(socket, payload_size);
  if (!payload.has_value()) {
    return std::nullopt;
  }

  auto checksum = DoubleSha256(*payload);
  if (!std::equal(checksum.begin(), checksum.begin() + 4U, header->begin() + 20U)) {
    return std::nullopt;
  }

  return DecodedMessage { std::move(command), std::move(*payload) };
}

auto SendMessage(
  TcpSocket& socket, std::string_view command, std::span<std::uint8_t const> payload)
  -> bool
{
  auto const message = EncodeMessage(command, payload);
  auto error = std::error_code {};
  asio::write(socket, asio::buffer(message), error);
  return !error;
}

auto ParseHeadersPayload(std::span<std::uint8_t const> payload,
  std::uint32_t locator_height,
  std::vector<std::string>& header_hashes,
  std::vector<Header>& headers) -> core::Status
{
  auto offset = std::size_t { 0 };
  auto const count = ReadCompactSize(payload, offset);
  if (!count.has_value() || *count == 0U) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "headers response is empty");
  }

  for (auto index = std::uint64_t { 0 }; index < *count; ++index) {
    if (offset + 80U > payload.size()) {
      return core::Status::Failure(
        core::StatusCode::Rejected, "headers payload is truncated");
    }

    auto const header = std::span<std::uint8_t const>(payload.data() + offset, 80U);
    offset += 80U;
    auto const txn_count = ReadCompactSize(payload, offset);
    if (!txn_count.has_value() || *txn_count != 0U) {
      return core::Status::Failure(
        core::StatusCode::Rejected, "headers payload contains invalid txn count");
    }

    auto version = std::uint32_t { 0 };
    std::memcpy(&version, header.data(), sizeof(version));
    auto const hash = DoubleSha256(header);
    auto reversed = std::array<std::uint8_t, 32> {};
    std::reverse_copy(hash.begin(), hash.end(), reversed.begin());
    auto const hash_hex = ToHex(reversed);
    header_hashes.push_back(hash_hex);
    headers.push_back(Header {
      .height = locator_height + static_cast<std::uint32_t>(index) + 1U,
      .hash_hex = hash_hex,
      .previous_hash_hex = WireHashToHex(
        std::span<std::uint8_t const>(header.data() + 4U, 32U)),
      .version = version,
    });
  }

  return core::Status::Success();
}

[[nodiscard]] auto ParseBlockPayload(std::span<std::uint8_t const> payload)
  -> std::optional<BlockBody>
{
  if (payload.size() < 80U) {
    return std::nullopt;
  }
  auto const header = std::span<std::uint8_t const>(payload.data(), 80U);
  auto const hash = DoubleSha256(header);
  auto reversed = std::array<std::uint8_t, 32> {};
  std::reverse_copy(hash.begin(), hash.end(), reversed.begin());
  return BlockBody {
    .block_hash_hex = ToHex(reversed),
    .payload = std::vector<std::uint8_t>(payload.begin(), payload.end()),
  };
}

[[nodiscard]] auto ParseBlockMetadata(
  std::span<std::uint8_t const> payload) -> std::optional<BlockMetadata>
{
  if (payload.size() < 81U) {
    return std::nullopt;
  }

  auto metadata = BlockMetadata {};
  std::memcpy(&metadata.version, payload.data(), sizeof(metadata.version));
  std::memcpy(&metadata.timestamp, payload.data() + 68U, sizeof(metadata.timestamp));
  std::memcpy(&metadata.nonce, payload.data() + 76U, sizeof(metadata.nonce));
  metadata.previous_hash_hex = WireHashToHex(
    std::span<std::uint8_t const>(payload.data() + 4U, 32U));

  auto const hash = DoubleSha256(std::span<std::uint8_t const>(payload.data(), 80U));
  auto reversed = std::array<std::uint8_t, 32> {};
  std::reverse_copy(hash.begin(), hash.end(), reversed.begin());
  metadata.block_hash_hex = ToHex(reversed);

  auto offset = std::size_t { 80U };
  auto const tx_count = ReadCompactSize(payload, offset);
  if (!tx_count.has_value()) {
    return std::nullopt;
  }
  metadata.transaction_count = *tx_count;
  return metadata;
}

[[nodiscard]] auto ImportStatePath(std::filesystem::path const& root)
  -> std::filesystem::path
{
  return root / "bitcoin-signet-import.txt";
}

[[nodiscard]] auto LoadImportState(std::filesystem::path const& root)
  -> std::optional<std::pair<std::uint32_t, std::string>>
{
  auto const path = ImportStatePath(root);
  if (!std::filesystem::exists(path)) {
    return std::nullopt;
  }

  auto input = std::ifstream(path);
  if (!input) {
    return std::nullopt;
  }

  auto height_line = std::string {};
  auto hash_line = std::string {};
  if (!std::getline(input, height_line) || !std::getline(input, hash_line)) {
    return std::nullopt;
  }

  auto const height_prefix = std::string_view { "height=" };
  auto const hash_prefix = std::string_view { "hash=" };
  if (!height_line.starts_with(height_prefix) || !hash_line.starts_with(hash_prefix)) {
    return std::nullopt;
  }

  return std::pair<std::uint32_t, std::string> {
    static_cast<std::uint32_t>(std::stoul(height_line.substr(height_prefix.size()))),
    hash_line.substr(hash_prefix.size()),
  };
}

} // namespace

HeaderSyncAdapter::HeaderSyncAdapter(Options options)
  : options_(std::move(options))
{
}

SignetLiveClient::SignetLiveClient(SignetLiveOptions options)
  : options_(std::move(options))
{
}

auto SignetLiveClient::FetchHeaders(SignetHeadersResult& result) -> core::Status
{
  auto io_context = asio::io_context {};
  auto resolver = asio::ip::tcp::resolver(io_context);
  auto error = std::error_code {};
  auto endpoints = resolver.resolve(options_.host,
    std::to_string(static_cast<unsigned int>(options_.port)), error);
  if (error) {
    return core::Status::Failure(
      core::StatusCode::IOError, "failed to resolve signet host");
  }

  auto const getheaders = EncodeGetHeadersPayload(options_.locator_hash_hex);
  if (!getheaders.has_value()) {
    return core::Status::Failure(
      core::StatusCode::InvalidArgument, "invalid signet locator hash");
  }

  for (auto const& endpoint : endpoints) {
    auto socket = TcpSocket(io_context);
    socket.connect(endpoint.endpoint(), error);
    if (error) {
      continue;
    }

    auto const remote_address = endpoint.endpoint().address().to_string();
    auto const version = EncodeVersionPayload(remote_address, options_.port);
    if (!SendMessage(socket, "version", version)) {
      continue;
    }

    auto saw_version = false;
    auto saw_verack = false;
    result = SignetHeadersResult {};
    while (!(saw_version && saw_verack)) {
      auto message = ReadMessage(socket);
      if (!message.has_value()) {
        break;
      }

      result.command_trace.push_back(message->command);
      if (message->command == "version") {
        if (message->payload.size() >= 4U) {
          std::memcpy(
            &result.protocol_version, message->payload.data(), sizeof(std::int32_t));
        }
        saw_version = true;
        if (!SendMessage(socket, "verack", std::span<std::uint8_t const> {})) {
          break;
        }
      } else if (message->command == "ping") {
        if (!SendMessage(socket, "pong", message->payload)) {
          break;
        }
      } else if (message->command == "verack") {
        saw_verack = true;
      }
    }

    if (!(saw_version && saw_verack)) {
      continue;
    }

    if (!SendMessage(socket, "getheaders", *getheaders)) {
      continue;
    }

    while (true) {
      auto message = ReadMessage(socket);
      if (!message.has_value()) {
        break;
      }

      result.command_trace.push_back(message->command);
      if (message->command == "ping") {
        if (!SendMessage(socket, "pong", message->payload)) {
          break;
        }
        continue;
      }
      if (message->command == "headers") {
        auto status = ParseHeadersPayload(message->payload, options_.locator_height,
          result.header_hashes, result.headers);
        if (!status.ok()) {
          return status;
        }
        result.peer_address = remote_address;
        return core::Status::Success();
      }
    }
  }

  return core::Status::Failure(
    core::StatusCode::IOError, "failed to complete live signet headers fetch");
}

auto SignetLiveClient::FetchBlocks(
  std::span<std::string const> block_hashes, SignetBlocksResult& result) -> core::Status
{
  if (block_hashes.empty()) {
    return core::Status::Failure(
      core::StatusCode::InvalidArgument, "at least one block hash is required");
  }

  auto const getdata = EncodeGetDataPayload(block_hashes);
  if (!getdata.has_value()) {
    return core::Status::Failure(
      core::StatusCode::InvalidArgument, "invalid block hash for getdata");
  }

  auto io_context = asio::io_context {};
  auto resolver = asio::ip::tcp::resolver(io_context);
  auto error = std::error_code {};
  auto endpoints = resolver.resolve(options_.host,
    std::to_string(static_cast<unsigned int>(options_.port)), error);
  if (error) {
    return core::Status::Failure(
      core::StatusCode::IOError, "failed to resolve signet host");
  }

  for (auto const& endpoint : endpoints) {
    auto socket = TcpSocket(io_context);
    socket.connect(endpoint.endpoint(), error);
    if (error) {
      continue;
    }

    auto const remote_address = endpoint.endpoint().address().to_string();
    auto const version = EncodeVersionPayload(remote_address, options_.port);
    if (!SendMessage(socket, "version", version)) {
      continue;
    }

    auto saw_version = false;
    auto saw_verack = false;
    result = SignetBlocksResult {};
    while (!(saw_version && saw_verack)) {
      auto message = ReadMessage(socket);
      if (!message.has_value()) {
        break;
      }

      result.command_trace.push_back(message->command);
      if (message->command == "version") {
        if (message->payload.size() >= 4U) {
          std::memcpy(
            &result.protocol_version, message->payload.data(), sizeof(std::int32_t));
        }
        saw_version = true;
        if (!SendMessage(socket, "verack", std::span<std::uint8_t const> {})) {
          break;
        }
      } else if (message->command == "ping") {
        if (!SendMessage(socket, "pong", message->payload)) {
          break;
        }
      } else if (message->command == "verack") {
        saw_verack = true;
      }
    }

    if (!(saw_version && saw_verack)) {
      continue;
    }

    if (!SendMessage(socket, "getdata", *getdata)) {
      continue;
    }

    while (result.blocks.size() < block_hashes.size()) {
      auto message = ReadMessage(socket);
      if (!message.has_value()) {
        break;
      }

      result.command_trace.push_back(message->command);
      if (message->command == "ping") {
        if (!SendMessage(socket, "pong", message->payload)) {
          break;
        }
        continue;
      }
      if (message->command == "block") {
        auto block = ParseBlockPayload(message->payload);
        if (!block.has_value()) {
          return core::Status::Failure(
            core::StatusCode::Rejected, "received malformed block payload");
        }
        auto metadata = ParseBlockMetadata(message->payload);
        if (!metadata.has_value()) {
          return core::Status::Failure(
            core::StatusCode::Rejected, "received block with unreadable metadata");
        }
        result.blocks.push_back(std::move(*block));
        result.metadata.push_back(std::move(*metadata));
      }
    }

    if (result.blocks.size() == block_hashes.size()) {
      result.peer_address = remote_address;
      return core::Status::Success();
    }
  }

  return core::Status::Failure(
    core::StatusCode::IOError, "failed to complete live signet block retrieval");
}

auto HeaderSyncAdapter::Bind(node::Node& node) -> core::Status
{
  node_ = &node;
  if (!options_.peer_hint.empty()) {
    if (auto status = node_->AttachDiscovery(BootstrapHint()); !status.ok()) {
      return status;
    }
  }

  return node_->AttachAdapter(AdapterName());
}

auto HeaderSyncAdapter::SubmitHeader(Header const& header) -> core::Status
{
  return SubmitHeaders(std::span<Header const>(&header, 1U));
}

auto HeaderSyncAdapter::SubmitHeaders(std::span<Header const> headers)
  -> core::Status
{
  if (node_ == nullptr) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "adapter must be bound to a node first");
  }
  if (headers.empty()) {
    return core::Status::Failure(
      core::StatusCode::InvalidArgument, "at least one header is required");
  }

  auto previous = last_header_;
  for (auto const& header : headers) {
    if (auto status = ValidateHeader(header, previous); !status.ok()) {
      return status;
    }
    previous = header;
  }

  auto imported = std::vector<std::uint32_t> {};
  imported.reserve(headers.size());
  for (auto const& header : headers) {
    auto payload = std::ostringstream {};
    payload << "height=" << header.height << ";hash=" << header.hash_hex
            << ";previous=" << header.previous_hash_hex
            << ";version=" << header.version;

    auto status = node_->SubmitTransaction(core::Transaction::FromText(
      "bitcoin.header", payload.str(), HeaderMetadata()));
    if (!status.ok()) {
      return status;
    }

    status = node_->CommitPending("bitcoin:" + NetworkName());
    if (!status.ok()) {
      return status;
    }

    imported.push_back(header.height);
  }

  imported_heights_.insert(
    imported_heights_.end(), imported.begin(), imported.end());
  last_header_ = headers.back();
  return core::Status::Success();
}

auto HeaderSyncAdapter::ImportLiveSignetHeaders(
  SignetLiveOptions options, SignetHeadersResult* live_result) -> core::Status
{
  if (node_ == nullptr) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "adapter must be bound to a node first");
  }

  auto resolved_options = ResolveImportOptions(std::move(options));
  auto client = SignetLiveClient(resolved_options);
  auto result = SignetHeadersResult {};
  if (auto status = client.FetchHeaders(result); !status.ok()) {
    return status;
  }
  if (result.headers.empty()) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "live signet fetch returned no headers");
  }

  auto status = SubmitHeaders(result.headers);
  if (status.ok()) {
    PersistImportProgress(resolved_options, result);
    if (live_result != nullptr) {
      *live_result = std::move(result);
    }
  }
  return status;
}

auto HeaderSyncAdapter::ResolveImportOptions(SignetLiveOptions options) const
  -> SignetLiveOptions
{
  if (options.state_root.empty()) {
    if (node_ != nullptr
      && node_->Options().storage_mode == node::StorageMode::FileSystem) {
      options.state_root = node_->Options().storage_root;
      if (options.state_root.empty()) {
        options.state_root = node_->Options().chain.data_directory;
      }
    }
  }

  if (options.state_root.empty()) {
    return options;
  }

  if (auto persisted = LoadImportState(options.state_root)) {
    options.locator_height = persisted->first;
    options.locator_hash_hex = persisted->second;
  }
  return options;
}

auto HeaderSyncAdapter::PersistImportProgress(
  SignetLiveOptions const& options, SignetHeadersResult const& result) const -> void
{
  if (options.state_root.empty() || result.headers.empty()) {
    return;
  }

  auto error = std::error_code {};
  std::filesystem::create_directories(options.state_root, error);
  if (error) {
    return;
  }

  auto output = std::ofstream(ImportStatePath(options.state_root), std::ios::trunc);
  if (!output) {
    return;
  }

  auto const& last = result.headers.back();
  output << "height=" << last.height << '\n';
  output << "hash=" << last.hash_hex << '\n';
}

auto HeaderSyncAdapter::ValidateHeader(
  Header const& header, std::optional<Header> const& previous) const -> core::Status
{
  if (header.hash_hex.empty()) {
    return core::Status::Failure(
      core::StatusCode::InvalidArgument, "header hash is required");
  }
  if (header.height > 0 && header.previous_hash_hex.empty()) {
    return core::Status::Failure(
      core::StatusCode::InvalidArgument, "non-genesis headers require a previous hash");
  }
  if (previous.has_value()) {
    if (header.height != previous->height + 1U) {
      return core::Status::Failure(core::StatusCode::Rejected,
        "headers must advance by exactly one height");
    }
    if (header.previous_hash_hex != previous->hash_hex) {
      return core::Status::Failure(core::StatusCode::Rejected,
        "header sequence is not continuous");
    }
  }
  return core::Status::Success();
}

auto HeaderSyncAdapter::HeaderMetadata() const -> std::string
{
  auto metadata = std::ostringstream {};
  metadata << "network=" << NetworkName() << ";mode=" << HeaderMode();
  if (!options_.peer_hint.empty()) {
    metadata << ";peer_hint=" << options_.peer_hint;
  }
  return metadata.str();
}

auto HeaderSyncAdapter::NetworkName() const -> std::string
{
  switch (options_.network) {
  case Network::Mainnet:
    return "mainnet";
  case Network::Testnet:
    return "testnet";
  case Network::Signet:
    return "signet";
  case Network::Regtest:
    return "regtest";
  }

  return "unknown";
}

auto HeaderSyncAdapter::HeaderMode() const -> std::string
{
  return options_.header_sync_only ? "headers-only" : "full";
}

auto HeaderSyncAdapter::AdapterName() const -> std::string
{
  return "bitcoin.header-sync:" + NetworkName() + ":" + HeaderMode();
}

auto HeaderSyncAdapter::BootstrapHint() const -> std::string
{
  return "bitcoin.peer:" + options_.peer_hint;
}

} // namespace blocxxi::bitcoin
