//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <Blocxxi/Bitcoin/api_export.h>

#include <cstdint>
#include <string>
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

class HeaderSyncAdapter {
public:
  explicit BLOCXXI_BITCOIN_API HeaderSyncAdapter(Options options = {});

  BLOCXXI_BITCOIN_API auto Bind(node::Node& node) -> core::Status;
  BLOCXXI_BITCOIN_API auto SubmitHeader(Header const& header) -> core::Status;

  [[nodiscard]] auto OptionsView() const -> Options const& { return options_; }
  [[nodiscard]] auto ImportedHeights() const -> std::vector<std::uint32_t>
  {
    return imported_heights_;
  }

private:
  [[nodiscard]] auto NetworkName() const -> std::string;

  Options options_ {};
  node::Node* node_ { nullptr };
  std::vector<std::uint32_t> imported_heights_ {};
};

} // namespace blocxxi::bitcoin
