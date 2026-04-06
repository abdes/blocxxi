//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/Bitcoin/adapter.h>

#include <sstream>
#include <utility>

#include <Blocxxi/Core/primitives.h>

namespace blocxxi::bitcoin {

HeaderSyncAdapter::HeaderSyncAdapter(Options options)
  : options_(std::move(options))
{
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
  if (node_ == nullptr) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "adapter must be bound to a node first");
  }
  if (header.hash_hex.empty()) {
    return core::Status::Failure(
      core::StatusCode::InvalidArgument, "header hash is required");
  }
  if (header.height > 0 && header.previous_hash_hex.empty()) {
    return core::Status::Failure(
      core::StatusCode::InvalidArgument, "non-genesis headers require a previous hash");
  }

  auto payload = std::ostringstream {};
  payload << "height=" << header.height << ";hash=" << header.hash_hex
          << ";previous=" << header.previous_hash_hex
          << ";version=" << header.version;

  auto metadata = std::ostringstream {};
  metadata << "network=" << NetworkName() << ";mode=" << HeaderMode();
  if (!options_.peer_hint.empty()) {
    metadata << ";peer_hint=" << options_.peer_hint;
  }

  auto status = node_->SubmitTransaction(core::Transaction::FromText(
    "bitcoin.header", payload.str(), metadata.str()));
  if (!status.ok()) {
    return status;
  }

  status = node_->CommitPending("bitcoin:" + NetworkName());
  if (status.ok()) {
    imported_heights_.push_back(header.height);
  }
  return status;
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
