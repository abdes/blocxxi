//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Blocxxi/Bitcoin/api_export.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Blocxxi/Core/result.h>
#include <Blocxxi/Node/node.h>

namespace blocxxi::bitcoin {

enum class Network {
  Mainnet,
  Testnet,
  Signet,
  Regtest,
};

struct Options {
  Network network { Network::Signet };
  std::string peer_hint {};
  bool header_sync_only { true };
};

struct Header {
  std::uint32_t height { 0 };
  std::string hash_hex {};
  std::string previous_hash_hex {};
  std::uint32_t version { 0 };
};

struct SignetLiveOptions {
  std::string host { "seed.signet.bitcoin.sprovoost.nl" };
  std::uint16_t port { 38333 };
  std::string locator_hash_hex {
    "00000008819873e925422c1ff0f99f7cc9bbb232af63a077a480a3633bee1ef6"
  };
  std::uint32_t locator_height { 0 };
  std::filesystem::path state_root {};
};

struct SignetHeadersResult {
  std::string peer_address {};
  std::int32_t protocol_version { 0 };
  std::vector<std::string> command_trace {};
  std::vector<std::string> header_hashes {};
  std::vector<Header> headers {};
};

struct BlockBody {
  std::string block_hash_hex {};
  std::vector<std::uint8_t> payload {};
};

struct SignetBlocksResult {
  std::string peer_address {};
  std::int32_t protocol_version { 0 };
  std::vector<std::string> command_trace {};
  std::vector<BlockBody> blocks {};
};

class SignetLiveClient {
public:
  explicit BLOCXXI_BITCOIN_API SignetLiveClient(SignetLiveOptions options = {});

  BLOCXXI_BITCOIN_API auto FetchHeaders(SignetHeadersResult& result)
    -> core::Status;
  BLOCXXI_BITCOIN_API auto FetchBlocks(std::span<std::string const> block_hashes,
    SignetBlocksResult& result) -> core::Status;

private:
  SignetLiveOptions options_ {};
};

class HeaderSyncAdapter {
public:
  explicit BLOCXXI_BITCOIN_API HeaderSyncAdapter(Options options = {});

  BLOCXXI_BITCOIN_API auto Bind(node::Node& node) -> core::Status;
  BLOCXXI_BITCOIN_API auto SubmitHeader(Header const& header) -> core::Status;
  BLOCXXI_BITCOIN_API auto SubmitHeaders(std::span<Header const> headers)
    -> core::Status;
  BLOCXXI_BITCOIN_API auto ImportLiveSignetHeaders(SignetLiveOptions options = {},
    SignetHeadersResult* live_result = nullptr) -> core::Status;

  [[nodiscard]] auto OptionsView() const -> Options const& { return options_; }
  [[nodiscard]] auto ImportedHeights() const -> std::vector<std::uint32_t>
  {
    return imported_heights_;
  }

private:
  [[nodiscard]] BLOCXXI_BITCOIN_API auto ResolveImportOptions(
    SignetLiveOptions options) const -> SignetLiveOptions;
  BLOCXXI_BITCOIN_API auto PersistImportProgress(
    SignetLiveOptions const& options, SignetHeadersResult const& result) const -> void;

  [[nodiscard]] BLOCXXI_BITCOIN_API auto ValidateHeader(Header const& header,
    std::optional<Header> const& previous) const -> core::Status;
  [[nodiscard]] BLOCXXI_BITCOIN_API auto HeaderMetadata() const -> std::string;

  [[nodiscard]] auto NetworkName() const -> std::string;
  [[nodiscard]] auto HeaderMode() const -> std::string;
  [[nodiscard]] auto AdapterName() const -> std::string;
  [[nodiscard]] auto BootstrapHint() const -> std::string;

  Options options_ {};
  node::Node* node_ { nullptr };
  std::vector<std::uint32_t> imported_heights_ {};
  std::optional<Header> last_header_ {};
};

} // namespace blocxxi::bitcoin
