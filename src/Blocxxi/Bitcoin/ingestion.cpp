//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at <https://opensource.org/licenses/BSD-3-Clause>.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <Blocxxi/Bitcoin/ingestion.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <asio.hpp>

#include <Blocxxi/Core/primitives.h>

namespace blocxxi::bitcoin {
namespace {

constexpr auto kBase64Alphabet
  = std::string_view { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" };

auto Base64Encode(std::string_view value) -> std::string
{
  auto output = std::string {};
  auto accumulator = std::uint32_t { 0 };
  auto bits = std::size_t { 0 };
  for (auto const ch : value) {
    accumulator = (accumulator << 8U) | static_cast<std::uint8_t>(ch);
    bits += 8U;
    while (bits >= 6U) {
      bits -= 6U;
      output.push_back(kBase64Alphabet[(accumulator >> bits) & 0x3FU]);
    }
  }

  if (bits > 0U) {
    accumulator <<= (6U - bits);
    output.push_back(kBase64Alphabet[accumulator & 0x3FU]);
  }

  while ((output.size() % 4U) != 0U) {
    output.push_back('=');
  }
  return output;
}

auto SkipWhitespace(std::string_view text, std::size_t position) -> std::size_t
{
  while (position < text.size()
    && std::isspace(static_cast<unsigned char>(text[position])) != 0) {
    position += 1U;
  }
  return position;
}

auto ConsumeString(std::string_view text, std::size_t position) -> std::optional<std::size_t>
{
  if (position >= text.size() || text[position] != '"') {
    return std::nullopt;
  }

  auto escaped = false;
  for (auto index = position + 1U; index < text.size(); ++index) {
    auto const current = text[index];
    if (escaped) {
      escaped = false;
      continue;
    }
    if (current == '\\') {
      escaped = true;
      continue;
    }
    if (current == '"') {
      return index + 1U;
    }
  }
  return std::nullopt;
}

auto ConsumeJsonValue(std::string_view text, std::size_t position)
  -> std::optional<std::size_t>
{
  position = SkipWhitespace(text, position);
  if (position >= text.size()) {
    return std::nullopt;
  }

  auto const first = text[position];
  if (first == '"') {
    return ConsumeString(text, position);
  }

  auto depth = std::size_t { 0 };
  auto in_string = false;
  auto escaped = false;
  for (auto index = position; index < text.size(); ++index) {
    auto const current = text[index];
    if (in_string) {
      if (escaped) {
        escaped = false;
        continue;
      }
      if (current == '\\') {
        escaped = true;
        continue;
      }
      if (current == '"') {
        in_string = false;
      }
      continue;
    }

    if (current == '"') {
      in_string = true;
      continue;
    }
    if (current == '{' || current == '[') {
      depth += 1U;
      continue;
    }
    if (current == '}' || current == ']') {
      if (depth == 0U) {
        return index;
      }
      depth -= 1U;
      if (depth == 0U) {
        return index + 1U;
      }
      continue;
    }
    if (depth == 0U && current == ',') {
      return index;
    }
  }

  return text.size();
}

auto Trim(std::string_view text) -> std::string_view
{
  auto first = std::size_t { 0 };
  auto last = text.size();
  while (first < last && std::isspace(static_cast<unsigned char>(text[first])) != 0) {
    first += 1U;
  }
  while (last > first
    && std::isspace(static_cast<unsigned char>(text[last - 1U])) != 0) {
    last -= 1U;
  }
  return text.substr(first, last - first);
}

auto ParseString(std::string_view raw) -> std::optional<std::string>
{
  raw = Trim(raw);
  if (raw.size() < 2U || raw.front() != '"' || raw.back() != '"') {
    return std::nullopt;
  }

  auto output = std::string {};
  output.reserve(raw.size() - 2U);
  auto escaped = false;
  for (auto index = std::size_t { 1U }; index + 1U < raw.size(); ++index) {
    auto const current = raw[index];
    if (escaped) {
      output.push_back(current);
      escaped = false;
      continue;
    }
    if (current == '\\') {
      escaped = true;
      continue;
    }
    output.push_back(current);
  }
  return output;
}

template <typename T>
auto ParseInteger(std::string_view raw) -> std::optional<T>
{
  raw = Trim(raw);
  auto value = T {};
  auto const* begin = raw.data();
  auto const* end = raw.data() + raw.size();
  auto const result = std::from_chars(begin, end, value);
  if (result.ec != std::errc() || result.ptr != end) {
    return std::nullopt;
  }
  return value;
}

auto ParseDouble(std::string_view raw) -> std::optional<double>
{
  auto text = std::string(Trim(raw));
  auto stream = std::stringstream(text);
  auto value = double { 0.0 };
  stream >> value;
  if (stream.fail()) {
    return std::nullopt;
  }
  return value;
}

auto ParseBool(std::string_view raw) -> std::optional<bool>
{
  raw = Trim(raw);
  if (raw == "true") {
    return true;
  }
  if (raw == "false") {
    return false;
  }
  return std::nullopt;
}

auto FindObjectValue(std::string_view object, std::string_view key)
  -> std::optional<std::string_view>
{
  object = Trim(object);
  if (object.size() < 2U || object.front() != '{' || object.back() != '}') {
    return std::nullopt;
  }

  auto position = std::size_t { 1U };
  while (position + 1U < object.size()) {
    position = SkipWhitespace(object, position);
    if (position >= object.size() || object[position] == '}') {
      break;
    }

    auto const key_end = ConsumeString(object, position);
    if (!key_end.has_value()) {
      return std::nullopt;
    }

    auto const current_key
      = ParseString(object.substr(position, *key_end - position));
    position = SkipWhitespace(object, *key_end);
    if (position >= object.size() || object[position] != ':') {
      return std::nullopt;
    }
    position = SkipWhitespace(object, position + 1U);
    auto const value_end = ConsumeJsonValue(object, position);
    if (!value_end.has_value()) {
      return std::nullopt;
    }

    if (current_key.has_value() && *current_key == key) {
      return Trim(object.substr(position, *value_end - position));
    }

    position = SkipWhitespace(object, *value_end);
    if (position < object.size() && object[position] == ',') {
      position += 1U;
    }
  }

  return std::nullopt;
}

auto ExtractResultValue(std::string_view response_json) -> std::optional<std::string_view>
{
  return FindObjectValue(response_json, "result");
}

template <typename T, typename Parser>
auto ParseResult(std::string const& response_json, Parser parser) -> std::optional<T>
{
  auto const raw = ExtractResultValue(response_json);
  if (!raw.has_value()) {
    return std::nullopt;
  }
  return parser(*raw);
}

auto ParseMempoolTransactions(std::string_view raw, std::size_t limit)
  -> std::optional<std::vector<TransactionObservation>>
{
  raw = Trim(raw);
  if (raw.size() < 2U || raw.front() != '{' || raw.back() != '}') {
    return std::nullopt;
  }

  auto observations = std::vector<TransactionObservation> {};
  auto position = std::size_t { 1U };
  while (position + 1U < raw.size() && observations.size() < limit) {
    position = SkipWhitespace(raw, position);
    if (position >= raw.size() || raw[position] == '}') {
      break;
    }

    auto const key_end = ConsumeString(raw, position);
    if (!key_end.has_value()) {
      return std::nullopt;
    }
    auto const txid = ParseString(raw.substr(position, *key_end - position));
    position = SkipWhitespace(raw, *key_end);
    if (position >= raw.size() || raw[position] != ':') {
      return std::nullopt;
    }
    position = SkipWhitespace(raw, position + 1U);
    auto const value_end = ConsumeJsonValue(raw, position);
    if (!value_end.has_value()) {
      return std::nullopt;
    }

    auto const entry = Trim(raw.substr(position, *value_end - position));
    auto observation = TransactionObservation {};
    observation.txid = txid.value_or(std::string {});
    if (auto const vsize = FindObjectValue(entry, "vsize")) {
      observation.vsize = ParseInteger<std::uint64_t>(*vsize).value_or(0U);
    }
    if (auto const weight = FindObjectValue(entry, "weight")) {
      observation.weight = ParseInteger<std::uint64_t>(*weight).value_or(0U);
    }
    if (auto const fees = FindObjectValue(entry, "fees")) {
      if (auto const base = FindObjectValue(*fees, "base")) {
        observation.base_fee_btc = ParseDouble(*base).value_or(0.0);
      }
    }
    observations.push_back(std::move(observation));

    position = SkipWhitespace(raw, *value_end);
    if (position < raw.size() && raw[position] == ',') {
      position += 1U;
    }
  }

  return observations;
}

auto ToSatoshis(double btc_value) -> std::uint64_t
{
  return static_cast<std::uint64_t>(btc_value * 100000000.0);
}

auto BuildRequest(std::string_view method, std::string_view params_json)
  -> std::string
{
  auto request = std::string {};
  request += R"({"jsonrpc":"1.0","id":"blocxxi","method":")";
  request += method;
  request += R"(","params":)";
  request += params_json;
  request += "}";
  return request;
}

} // namespace

RpcTransport::~RpcTransport() = default;
DataSourceAdapter::~DataSourceAdapter() = default;

HttpRpcTransport::HttpRpcTransport(RpcConnectionConfig config)
  : config_(std::move(config))
{
}

HttpRpcTransport::~HttpRpcTransport() = default;

auto HttpRpcTransport::Call(std::string_view method, std::string_view params_json,
  std::string& response_json) -> core::Status
{
  if (config_.host.empty() || config_.username.empty() || config_.password.empty()) {
    return core::Status::Failure(
      core::StatusCode::InvalidArgument,
      "rpc transport requires host, username, and password");
  }

  auto io_context = asio::io_context {};
  auto resolver = asio::ip::tcp::resolver(io_context);
  auto socket = asio::ip::tcp::socket(io_context);
  auto error = std::error_code {};
  auto const endpoints = resolver.resolve(
    config_.host, std::to_string(config_.port), error);
  if (error) {
    return core::Status::Failure(core::StatusCode::IOError, error.message());
  }

  asio::connect(socket, endpoints, error);
  if (error) {
    return core::Status::Failure(core::StatusCode::IOError, error.message());
  }

  auto const body = BuildRequest(method, params_json);
  auto request = std::string {};
  request += "POST " + config_.path + " HTTP/1.1\r\n";
  request += "Host: " + config_.host + ":" + std::to_string(config_.port) + "\r\n";
  request += "Authorization: Basic "
    + Base64Encode(config_.username + ":" + config_.password) + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Connection: close\r\n";
  request += "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
  request += body;

  asio::write(socket, asio::buffer(request), error);
  if (error) {
    return core::Status::Failure(core::StatusCode::IOError, error.message());
  }

  auto response = std::string {};
  auto buffer = std::array<char, 4096> {};
  while (true) {
    auto const read = socket.read_some(asio::buffer(buffer), error);
    if (error == asio::error::eof) {
      break;
    }
    if (error) {
      return core::Status::Failure(core::StatusCode::IOError, error.message());
    }
    response.append(buffer.data(), read);
  }

  auto const separator = response.find("\r\n\r\n");
  if (separator == std::string::npos) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "rpc response missing http body");
  }
  if (response.rfind("HTTP/1.1 200", 0U) != 0U
    && response.rfind("HTTP/1.0 200", 0U) != 0U) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "rpc transport returned non-200 response");
  }

  response_json = response.substr(separator + 4U);
  return core::Status::Success();
}

BitcoinCoreRpcAdapter::BitcoinCoreRpcAdapter(
  BitcoinCoreRpcConfig config, std::shared_ptr<RpcTransport> transport)
  : config_(std::move(config))
  , transport_(std::move(transport))
{
  if (!transport_) {
    transport_ = std::make_shared<HttpRpcTransport>(config_.connection);
  }
}

BitcoinCoreRpcAdapter::~BitcoinCoreRpcAdapter() = default;

auto BitcoinCoreRpcAdapter::Name() const -> std::string
{
  return "bitcoin.core-rpc";
}

auto BitcoinCoreRpcAdapter::Poll(ObservationBatch& batch) -> core::Status
{
  if (!transport_) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "rpc transport is not configured");
  }

  batch = ObservationBatch {};
  batch.source_name = Name();
  batch.network = config_.network;
  batch.observed_at_utc = core::NowUnixSeconds();

  auto response = std::string {};
  auto status = transport_->Call("getblockcount", "[]", response);
  if (!status.ok()) {
    return status;
  }
  auto const block_count
    = ParseResult<std::uint64_t>(response, ParseInteger<std::uint64_t>);
  if (!block_count.has_value()) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "failed to parse getblockcount result");
  }

  status = transport_->Call("getbestblockhash", "[]", response);
  if (!status.ok()) {
    return status;
  }
  auto const best_hash = ParseResult<std::string>(response, ParseString);
  if (!best_hash.has_value()) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "failed to parse getbestblockhash result");
  }

  status = transport_->Call("getmempoolinfo", "[]", response);
  if (!status.ok()) {
    return status;
  }
  auto const mempool_info = ExtractResultValue(response);
  if (!mempool_info.has_value()) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "failed to parse getmempoolinfo result");
  }

  auto network = NetworkHealthObservation {};
  network.mempool_transaction_count
    = ParseInteger<std::uint64_t>(FindObjectValue(*mempool_info, "size").value_or("0"))
        .value_or(0U);
  network.mempool_bytes
    = ParseInteger<std::uint64_t>(FindObjectValue(*mempool_info, "bytes").value_or("0"))
        .value_or(0U);

  status = transport_->Call("getnetworkinfo", "[]", response);
  if (!status.ok()) {
    return status;
  }
  auto const network_info = ExtractResultValue(response);
  if (!network_info.has_value()) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "failed to parse getnetworkinfo result");
  }
  network.connection_count = ParseInteger<std::uint64_t>(
    FindObjectValue(*network_info, "connections").value_or("0")).value_or(0U);
  network.relay_fee_btc = ParseDouble(
    FindObjectValue(*network_info, "relayfee").value_or("0")).value_or(0.0);
  network.warning = ParseString(
    FindObjectValue(*network_info, "warnings").value_or("\"\"")).value_or("");

  status = transport_->Call("getblockchaininfo", "[]", response);
  if (!status.ok()) {
    return status;
  }
  auto const blockchain_info = ExtractResultValue(response);
  if (!blockchain_info.has_value()) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "failed to parse getblockchaininfo result");
  }
  network.initial_block_download = ParseBool(
    FindObjectValue(*blockchain_info, "initialblockdownload").value_or("false"))
                                     .value_or(false);

  status = transport_->Call("getrawmempool", "[true]", response);
  if (!status.ok()) {
    return status;
  }
  auto const mempool_entries = ExtractResultValue(response);
  if (!mempool_entries.has_value()) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "failed to parse getrawmempool result");
  }
  auto const parsed_entries
    = ParseMempoolTransactions(*mempool_entries, config_.max_mempool_transactions);
  if (!parsed_entries.has_value()) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "failed to parse verbose mempool entries");
  }

  status = transport_->Call(
    "getblockstats", "[" + std::to_string(*block_count) + ", [\"txs\", \"totalfee\"]]",
    response);
  if (!status.ok()) {
    return status;
  }
  auto const block_stats = ExtractResultValue(response);
  if (!block_stats.has_value()) {
    return core::Status::Failure(
      core::StatusCode::Rejected, "failed to parse getblockstats result");
  }

  auto block = BlockObservation {};
  block.height = *block_count;
  block.block_hash_hex = *best_hash;
  block.transaction_count = ParseInteger<std::uint64_t>(
    FindObjectValue(*block_stats, "txs").value_or("0")).value_or(0U);
  block.total_fee_satoshis = ToSatoshis(ParseDouble(
    FindObjectValue(*block_stats, "totalfee").value_or("0")).value_or(0.0));

  batch.latest_block = std::move(block);
  batch.network_health = std::move(network);
  batch.mempool_transactions = *parsed_entries;
  return core::Status::Success();
}

} // namespace blocxxi::bitcoin
