//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#ifndef BLOCXXI_P2P_KADEMLIA_SESSION_H_
#define BLOCXXI_P2P_KADEMLIA_SESSION_H_

#include <system_error>

#include <boost/asio/io_context.hpp>

#include <common/logging.h>
#include <p2p/kademlia/key.h>

namespace blocxxi {
namespace p2p {
namespace kademlia {

template <typename TEngine>
class Session final : blocxxi::logging::Loggable<logging::Id::P2P_KADEMLIA> {
 public:
  using EngineType = TEngine;
  ///
  using DataType = std::vector<std::uint8_t>;
  ///
  using StoreHandlerType = std::function<void(std::error_code const &error)>;
  ///
  using LoadHandlerType =
      std::function<void(std::error_code const &error, DataType const &data)>;

 public:
  explicit Session(boost::asio::io_context &io_context, EngineType &&engine)
      : io_context_(io_context), engine_(std::move(engine)) {
    BXLOG(debug, "Creating Session DONE");
  }

  Session(Session const &) = delete;
  Session &operator=(Session const &) = delete;

  Session(Session &&) = default;
  Session &operator=(Session &&) = default;

  ~Session() { BXLOG(debug, "Destroy Session"); }

  EngineType const &GetEngine() const { return engine_; }

  void Start() {
    BXLOG(debug, "Session Start");
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
    BXLOG(debug, "Session Stop");
    engine_.Stop();
  }

  void StoreValue(KeyType const &key, DataType const &data,
                  StoreHandlerType handler) {
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

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi

#endif  // BLOCXXI_P2P_KADEMLIA_SESSION_H_
