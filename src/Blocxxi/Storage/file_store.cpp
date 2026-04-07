//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/Storage/file_store.h>

#include <algorithm>
#include <fstream>
#include <system_error>

#include <Blocxxi/Codec/base16.h>

namespace blocxxi::storage {
namespace {

class FileBlockStore final : public chain::BlockStore {
public:
  explicit FileBlockStore(std::filesystem::path root_directory)
    : root_directory_(std::move(root_directory))
  {
  }

  auto PutBlock(core::Block const& block) -> core::Status override;
  [[nodiscard]] auto GetBlock(core::BlockId const& id) const
    -> std::optional<core::Block> override;
  [[nodiscard]] auto GetChain() const -> std::vector<core::Block> override;

private:
  [[nodiscard]] auto BlocksDirectory() const -> std::filesystem::path;
  std::filesystem::path root_directory_ {};
};

class FileSnapshotStore final : public chain::SnapshotStore {
public:
  explicit FileSnapshotStore(std::filesystem::path root_directory)
    : root_directory_(std::move(root_directory))
  {
  }

  auto Save(core::ChainSnapshot const& snapshot) -> core::Status override;
  [[nodiscard]] auto Load() const -> std::optional<core::ChainSnapshot> override;

private:
  [[nodiscard]] auto SnapshotPath() const -> std::filesystem::path;
  std::filesystem::path root_directory_ {};
};

[[nodiscard]] auto EncodeString(std::string_view value) -> std::string
{
  auto const bytes
    = std::span<std::uint8_t const>(reinterpret_cast<std::uint8_t const*>(value.data()), value.size());
  return blocxxi::codec::hex::Encode(bytes, false, true);
}

[[nodiscard]] auto DecodeString(std::string_view hex_value) -> std::string
{
  auto bytes = std::vector<std::uint8_t>(hex_value.size() / 2U);
  if (!hex_value.empty()) {
    blocxxi::codec::hex::Decode(std::string(hex_value), std::span<std::uint8_t>(bytes.data(), bytes.size()));
  }
  return std::string(bytes.begin(), bytes.end());
}

[[nodiscard]] auto BlockPath(std::filesystem::path const& root,
  core::Block const& block) -> std::filesystem::path
{
  auto file_name = std::to_string(block.header.height) + "-" + block.header.id.ToHex() + ".blk";
  return root / "blocks" / file_name;
}

[[nodiscard]] auto ParseLine(std::string const& line) -> std::pair<std::string, std::string>
{
  auto const separator = line.find('=');
  if (separator == std::string::npos) {
    return { line, {} };
  }
  return { line.substr(0, separator), line.substr(separator + 1) };
}

[[nodiscard]] auto ParseTransaction(std::string const& line) -> core::Transaction
{
  auto parts = std::vector<std::string> {};
  auto start = std::size_t { 0 };
  while (start <= line.size()) {
    auto const next = line.find('|', start);
    if (next == std::string::npos) {
      parts.push_back(line.substr(start));
      break;
    }
    parts.push_back(line.substr(start, next - start));
    start = next + 1;
  }

  auto payload = std::vector<std::uint8_t>(parts[2].size() / 2U);
  if (!parts[2].empty()) {
    blocxxi::codec::hex::Decode(parts[2], std::span<std::uint8_t>(payload.data(), payload.size()));
  }

  return core::Transaction {
    .id = core::TransactionId::FromHex(parts[0]),
    .type = DecodeString(parts[1]),
    .payload = std::move(payload),
    .metadata = DecodeString(parts[3]),
  };
}

[[nodiscard]] auto ReadBlock(std::filesystem::path const& path)
  -> std::optional<core::Block>
{
  auto input = std::ifstream(path);
  if (!input) {
    return std::nullopt;
  }

  auto block = core::Block {};
  auto line = std::string {};
  while (std::getline(input, line)) {
    if (line.empty()) {
      continue;
    }
    auto const [key, value] = ParseLine(line);
    if (key == "id") {
      block.header.id = core::BlockId::FromHex(value);
    } else if (key == "previous") {
      block.header.previous_id = core::BlockId::FromHex(value);
    } else if (key == "height") {
      block.header.height = static_cast<core::Height>(std::stoull(value));
    } else if (key == "timestamp") {
      block.header.timestamp_utc = std::stoll(value);
    } else if (key == "source") {
      block.header.source = DecodeString(value);
    } else if (key == "tx") {
      block.transactions.push_back(ParseTransaction(value));
    }
  }

  return block;
}
} // namespace

auto FileBlockStore::BlocksDirectory() const -> std::filesystem::path
{
  return root_directory_ / "blocks";
}

auto FileBlockStore::PutBlock(core::Block const& block) -> core::Status
{
  auto error = std::error_code {};
  std::filesystem::create_directories(BlocksDirectory(), error);
  if (error) {
    return core::Status::Failure(
      core::StatusCode::IOError, "failed to create block store directory");
  }

  auto const path = BlockPath(root_directory_, block);
  auto output = std::ofstream(path, std::ios::trunc);
  if (!output) {
    return core::Status::Failure(
      core::StatusCode::IOError, "failed to open block file for writing");
  }

  output << "id=" << block.header.id.ToHex() << '\n';
  output << "previous=" << block.header.previous_id.ToHex() << '\n';
  output << "height=" << block.header.height << '\n';
  output << "timestamp=" << block.header.timestamp_utc << '\n';
  output << "source=" << EncodeString(block.header.source) << '\n';
  for (auto const& transaction : block.transactions) {
    output << "tx=" << transaction.id.ToHex() << '|'
           << EncodeString(transaction.type) << '|'
           << blocxxi::codec::hex::Encode(
                std::span<std::uint8_t const>(transaction.payload.data(), transaction.payload.size()),
                false, true)
           << '|'
           << EncodeString(transaction.metadata) << '\n';
  }

  if (!output.good()) {
    return core::Status::Failure(
      core::StatusCode::IOError, "failed to persist block contents");
  }

  return core::Status::Success();
}

auto FileBlockStore::GetBlock(core::BlockId const& id) const
  -> std::optional<core::Block>
{
  auto const blocks_dir = BlocksDirectory();
  if (!std::filesystem::exists(blocks_dir)) {
    return std::nullopt;
  }

  for (auto const& entry : std::filesystem::directory_iterator(blocks_dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    if (auto block = ReadBlock(entry.path()); block && block->header.id == id) {
      return block;
    }
  }

  return std::nullopt;
}

auto FileBlockStore::GetChain() const -> std::vector<core::Block>
{
  auto chain = std::vector<core::Block> {};
  auto const blocks_dir = BlocksDirectory();
  if (!std::filesystem::exists(blocks_dir)) {
    return chain;
  }

  auto files = std::vector<std::filesystem::path> {};
  for (auto const& entry : std::filesystem::directory_iterator(blocks_dir)) {
    if (entry.is_regular_file()) {
      files.push_back(entry.path());
    }
  }
  std::ranges::sort(files);

  for (auto const& file : files) {
    if (auto block = ReadBlock(file)) {
      chain.push_back(*block);
    }
  }
  return chain;
}
auto FileSnapshotStore::SnapshotPath() const -> std::filesystem::path
{
  return root_directory_ / "snapshot.txt";
}

auto FileSnapshotStore::Save(core::ChainSnapshot const& snapshot) -> core::Status
{
  auto error = std::error_code {};
  std::filesystem::create_directories(root_directory_, error);
  if (error) {
    return core::Status::Failure(
      core::StatusCode::IOError, "failed to create snapshot directory");
  }

  auto output = std::ofstream(SnapshotPath(), std::ios::trunc);
  if (!output) {
    return core::Status::Failure(
      core::StatusCode::IOError, "failed to open snapshot file for writing");
  }

  output << "height=" << snapshot.height << '\n';
  output << "head=" << snapshot.head_id.ToHex() << '\n';
  output << "block_count=" << snapshot.block_count << '\n';
  output << "accepted_transactions=" << snapshot.accepted_transactions << '\n';
  output << "bootstrapped=" << (snapshot.bootstrapped ? 1 : 0) << '\n';

  return output.good()
    ? core::Status::Success()
    : core::Status::Failure(
        core::StatusCode::IOError, "failed to persist snapshot contents");
}

auto FileSnapshotStore::Load() const -> std::optional<core::ChainSnapshot>
{
  auto input = std::ifstream(SnapshotPath());
  if (!input) {
    return std::nullopt;
  }

  auto snapshot = core::ChainSnapshot {};
  auto line = std::string {};
  while (std::getline(input, line)) {
    auto const [key, value] = ParseLine(line);
    if (key == "height") {
      snapshot.height = static_cast<core::Height>(std::stoull(value));
    } else if (key == "head") {
      snapshot.head_id = core::BlockId::FromHex(value);
    } else if (key == "block_count") {
      snapshot.block_count = std::stoull(value);
    } else if (key == "accepted_transactions") {
      snapshot.accepted_transactions = std::stoull(value);
    } else if (key == "bootstrapped") {
      snapshot.bootstrapped = value == "1";
    }
  }

  return snapshot;
}

auto MakeFileBlockStore(std::filesystem::path root_directory)
  -> std::shared_ptr<chain::BlockStore>
{
  return std::make_shared<FileBlockStore>(std::move(root_directory));
}

auto MakeFileSnapshotStore(std::filesystem::path root_directory)
  -> std::shared_ptr<chain::SnapshotStore>
{
  return std::make_shared<FileSnapshotStore>(std::move(root_directory));
}

} // namespace blocxxi::storage
