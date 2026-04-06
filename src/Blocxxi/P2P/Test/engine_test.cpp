//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <memory>
#include <type_traits>
#include <vector>

#include <Nova/Base/Compilers.h>
#include <gtest/gtest.h>

#include <Blocxxi/P2P/kademlia/engine.h>

// Disable compiler and linter warnings originating from the unit test framework
// and for which we cannot do anything. Additionally, every TEST or TEST_X macro
// usage must be preceded by a '// NOLINTNEXTLINE'.
NOVA_DIAGNOSTIC_PUSH
#if NOVA_CLANG_VERSION
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

namespace blocxxi::p2p::kademlia {

namespace {

struct FakeBucket {
  Node node_ { Node::IdType::RandomHash(), "127.0.0.1", 40000 };

  [[nodiscard]] auto Empty() const -> bool { return false; }
  [[nodiscard]] auto LeastRecentlySeenNode() const -> Node const& { return node_; }
  [[nodiscard]] auto TimeSinceLastUpdated() const -> std::chrono::seconds
  {
    return std::chrono::seconds(0);
  }
  [[nodiscard]] auto SelectRandomNode() const -> Node const& { return node_; }
};

struct FakeRoutingTable {
  using Storage = std::vector<FakeBucket>;
  using const_iterator = Storage::const_iterator;

  Node this_node_ { Node::IdType::RandomHash(), "127.0.0.1", 39999 };
  Storage buckets_ { FakeBucket {} };

  [[nodiscard]] auto ThisNode() const -> Node const& { return this_node_; }
  [[nodiscard]] auto Empty() const -> bool { return true; }
  [[nodiscard]] auto BucketsCount() const -> std::size_t { return buckets_.size(); }
  [[nodiscard]] auto cbegin() const -> const_iterator { return buckets_.cbegin(); }
  [[nodiscard]] auto cend() const -> const_iterator { return buckets_.cend(); }
  [[nodiscard]] auto begin() const -> const_iterator { return buckets_.cbegin(); }
  [[nodiscard]] auto end() const -> const_iterator { return buckets_.cend(); }
  auto AddPeer(Node&&) -> bool { return true; }
  [[nodiscard]] auto GetBucketIndexFor(Node::IdType const&) const -> std::size_t
  {
    return 0;
  }
  [[nodiscard]] auto FindNeighbors(Node::IdType const&) const -> std::vector<Node>
  {
    return {};
  }
  [[nodiscard]] auto FindNeighbors(Node::IdType const&, std::size_t) const
    -> std::vector<Node>
  {
    return {};
  }
  auto PeerTimedOut(Node const&) -> bool { return false; }
  void DumpToLog() const { }
};

struct FakeNetwork {
  using EndpointType = IpEndpoint;
  using Handler = std::function<void(EndpointType const&, BufferReader const&)>;

  explicit FakeNetwork(std::shared_ptr<int> send_count)
    : send_count_(std::move(send_count))
  {
  }

  FakeNetwork(FakeNetwork&&) noexcept = default;
  auto operator=(FakeNetwork&&) noexcept -> FakeNetwork& = default;

  void OnMessageReceived(Handler handler) { handler_ = std::move(handler); }
  void Start() { started_ = true; }
  void HandleNewResponse(EndpointType const&, Header const&, BufferReader const&)
  {
  }

  template <typename Request, typename Response, typename Error>
  void SendConvRequest(Request const&, EndpointType const&,
    Timer::DurationType const&, Response const&, Error const&)
  {
    ++(*send_count_);
  }

  template <typename Response>
  void SendResponse(blocxxi::crypto::Hash160 const&, Response const&,
    EndpointType const&)
  {
  }

  std::shared_ptr<int> send_count_;
  Handler handler_;
  bool started_ { false };
};

using EngineUnderTest = Engine<FakeRoutingTable, FakeNetwork>;

static_assert(std::is_same_v<
                decltype(std::declval<EngineUnderTest const&>().GetRoutingTable()),
                FakeRoutingTable const&>);

} // namespace

// NOLINTNEXTLINE
TEST(EngineTest, StopCancelsRefreshTimerBeforePingTaskRuns)
{
  auto io_context = asio::io_context {};
  auto send_count = std::make_shared<int>(0);
  auto engine = EngineUnderTest(
    io_context, FakeRoutingTable {}, FakeNetwork { send_count });

  engine.Start();
  engine.Stop();

  io_context.run_for(PERIODIC_REFRESH_TIMER + std::chrono::seconds(1));

  ASSERT_EQ(*send_count, 0);
}

} // namespace blocxxi::p2p::kademlia
NOVA_DIAGNOSTIC_POP
