//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/Core/primitives.h>

#include <array>
#include <chrono>

namespace blocxxi::core {
namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

[[nodiscard]] auto StableHash(std::string_view seed, std::uint64_t lane)
  -> std::uint64_t
{
  auto value = kFnvOffset ^ (lane + 0x9e3779b97f4a7c15ULL);
  for (auto const byte : seed) {
    value ^= static_cast<std::uint8_t>(byte);
    value *= kFnvPrime;
  }
  value ^= lane;
  value *= kFnvPrime;
  return value;
}

} // namespace

auto Transaction::FromText(
  std::string type, std::string payload, std::string metadata) -> Transaction
{
  auto payload_bytes = ToBytes(payload);
  auto const seed = type + "|" + payload + "|" + metadata;
  return Transaction {
    .id = MakeId(seed),
    .type = std::move(type),
    .payload = std::move(payload_bytes),
    .metadata = std::move(metadata),
  };
}

auto Transaction::PayloadText() const -> std::string
{
  return ToString(payload);
}

auto Block::MakeNext(BlockId previous_id,
  Height height,
  std::vector<Transaction> transactions,
  std::string source) -> Block
{
  auto seed = std::string {};
  seed.reserve(256);
  seed += previous_id.ToHex();
  seed += "|" + std::to_string(height) + "|" + source;
  for (auto const& transaction : transactions) {
    seed += "|" + transaction.id.ToHex();
  }

  return Block {
    .header = BlockHeader {
      .id = MakeId(seed),
      .previous_id = previous_id,
      .height = height,
      .timestamp_utc = NowUnixSeconds(),
      .source = std::move(source),
    },
    .transactions = std::move(transactions),
  };
}

auto MakeId(std::string_view seed) -> blocxxi::crypto::Hash256
{
  auto raw = std::array<std::uint8_t, 32> {};
  for (std::size_t lane = 0; lane < 4; ++lane) {
    auto const value = StableHash(seed, lane);
    for (std::size_t offset = 0; offset < 8; ++offset) {
      raw[lane * 8 + offset]
        = static_cast<std::uint8_t>((value >> ((7 - offset) * 8U)) & 0xFFU);
    }
  }
  return blocxxi::crypto::Hash256(std::span<std::uint8_t const>(raw.data(), raw.size()));
}

auto ToBytes(std::string_view text) -> ByteVector
{
  return ByteVector(text.begin(), text.end());
}

auto ToString(ByteVector const& bytes) -> std::string
{
  return std::string(bytes.begin(), bytes.end());
}

auto NowUnixSeconds() -> std::int64_t
{
  auto const now = std::chrono::system_clock::now();
  auto const seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
  return seconds.time_since_epoch().count();
}

} // namespace blocxxi::core
