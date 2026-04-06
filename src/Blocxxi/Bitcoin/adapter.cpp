//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/Bitcoin/adapter.h>

#include <sstream>
#include <span>
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
