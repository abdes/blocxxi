//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <system_error>

#include <Blocxxi/P2P/kademlia/asio.h>

#include <Nova/Base/Logging.h>

#include <Blocxxi/P2P/kademlia/key.h>

namespace blocxxi::p2p::kademlia {

template <typename TEngine> class LegacySession final {
public:
  static constexpr const char* LOGGER_NAME = "p2p-kademlia";

  using EngineType = TEngine;
  using DataType = std::vector<std::uint8_t>;
  using StoreHandlerType = std::function<void(std::error_code const& error)>;
  using LoadHandlerType
    = std::function<void(std::error_code const& error, DataType const& data)>;

  explicit LegacySession(asio::io_context& io_context, EngineType&& engine)
    : io_context_(io_context)
    , engine_(std::move(engine))
  {
    LOG_F(1, "Creating LegacySession DONE");
  }

  LegacySession(LegacySession const&) = delete;
  auto operator=(LegacySession const&) -> LegacySession& = delete;

  LegacySession(LegacySession&&) noexcept = default;
  auto operator=(LegacySession&&) noexcept -> LegacySession& = default;

  ~LegacySession() { LOG_F(1, "Destroy LegacySession"); }

  auto GetEngine() const -> EngineType const& { return engine_; }

  void Start()
  {
    LOG_F(1, "LegacySession Start");
    engine_.Start();
  }

  void Stop()
  {
    LOG_F(1, "LegacySession Stop");
    engine_.Stop();
  }

  void StoreValue(
    KeyType const& key, DataType const& data, StoreHandlerType handler)
  {
    engine_.AsyncStoreValue(key, data, handler);
  }

  void FindValue(KeyType const& key, LoadHandlerType handler)
  {
    engine_.AsyncFindValue(key, handler);
  }

private:
  asio::io_context& io_context_;
  EngineType engine_;
};

} // namespace blocxxi::p2p::kademlia
