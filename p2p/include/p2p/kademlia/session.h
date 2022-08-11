//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <system_error>

#include <boost/asio/io_context.hpp>

#include <logging/logging.h>

#include <p2p/kademlia/key.h>

namespace blocxxi::p2p::kademlia {

template <typename TEngine>
class Session final : asap::logging::Loggable<Session<TEngine>> {
public:
  /// The logger id used for logging within this class.
  static constexpr const char *LOGGER_NAME = "p2p-kademlia";

  // We need to import the internal logger retrieval method symbol in this
  // context to avoid g++ complaining about the method not being declared before
  // being used. THis is due to the fact that the current class is a template
  // class and that method does not take any template argument that will enable
  // the compiler to resolve it unambiguously.
  using asap::logging::Loggable<
      Session<TEngine>>::internal_log_do_not_use_read_comment;

  using EngineType = TEngine;
  ///
  using DataType = std::vector<std::uint8_t>;
  ///
  using StoreHandlerType = std::function<void(std::error_code const &error)>;
  ///
  using LoadHandlerType =
      std::function<void(std::error_code const &error, DataType const &data)>;

  explicit Session(boost::asio::io_context &io_context, EngineType &&engine)
      : io_context_(io_context), engine_(std::move(engine)) {
    ASLOG(debug, "Creating Session DONE");
  }

  Session(Session const &) = delete;
  auto operator=(Session const &) -> Session & = delete;

  Session(Session &&) noexcept = default;
  auto operator=(Session &&) noexcept -> Session & = default;

  ~Session() {
    ASLOG(debug, "Destroy Session");
  }

  auto GetEngine() const -> EngineType const & {
    return engine_;
  }

  void Start() {
    ASLOG(debug, "Session Start");
    engine_.Start();
  }

  /*  void RunOne() {
      engine_.RunOne();
    }

    void RunAll() {
      engine_.RunAll();
    }
  */
  void Stop() {
    ASLOG(debug, "Session Stop");
    engine_.Stop();
  }

  void StoreValue(
      KeyType const &key, DataType const &data, StoreHandlerType handler) {
    engine_.AsyncStoreValue(key, data, handler);
  }

  void FindValue(KeyType const &key, LoadHandlerType handler) {
    engine_.AsyncFindValue(key, handler);
  }

private:
  ///
  boost::asio::io_context &io_context_;
  ///
  EngineType engine_;
};

} // namespace blocxxi::p2p::kademlia
