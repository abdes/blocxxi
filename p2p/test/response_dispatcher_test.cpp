//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include <p2p/kademlia/response_dispatcher.h>

namespace blocxxi::p2p::kademlia {

// NOLINTNEXTLINE
TEST(ResponseDispatcherTest, BasicOperation) {
  boost::asio::io_context io_context;
  auto dispatcher = ResponseDispatcher(io_context);
  auto hash = KeyType::RandomHash();
  {
    dispatcher.RegisterCallbackWithTimeout(
        hash, std::chrono::seconds(5),
        [hash](ResponseDispatcher::EndpointType const &sender,
            Header const &header, BufferReader const &) {
          ASSERT_EQ(hash, header.random_token_);
          ASSERT_EQ("92.54.32.1", sender.Address().to_string());
          ASSERT_EQ(4242, sender.Port());
        },
        [](std::error_code const &failure) {
          FAIL() << "unexpected error: " << failure.message();
        });
  }

  Header header;
  header.random_token_ = hash;
  Buffer buffer;
  dispatcher.HandleResponse(
      ResponseDispatcher::EndpointType(
          ResponseDispatcher::EndpointType::AddressType::from_string(
              "92.54.32.1"),
          4242),
      header, BufferReader(buffer));
}

// NOLINTNEXTLINE
TEST(ResponseDispatcherTest, CallbackTimeout) {
  boost::asio::io_context io_context;
  auto dispatcher = ResponseDispatcher(io_context);
  auto hash = KeyType::RandomHash();
  {
    dispatcher.RegisterCallbackWithTimeout(
        hash, std::chrono::seconds(5),
        [hash](ResponseDispatcher::EndpointType const &, Header const &,
            BufferReader const &) { FAIL() << "should have timed out"; },
        [](std::error_code const &) { SUCCEED(); });
  }

  io_context.run();
}

} // namespace blocxxi::p2p::kademlia
