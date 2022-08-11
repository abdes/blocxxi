//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#pragma once

#include <logging/logging.h>

#include <p2p/kademlia/node.h>

#include <map>
#include <ostream>
#include <utility>
#include <vector>

namespace blocxxi::p2p::kademlia::detail {

class BaseLookupTask : protected asap::logging::Loggable<BaseLookupTask> {
public:
  /// The logger id used for logging within this class.
  static constexpr const char *LOGGER_NAME = "p2p-kademlia";

  // We need to import the internal logger retrieval method symbol in this
  // context to avoid g++ complaining about the method not being declared before
  // being used. THis is due to the fact that the current class is a template
  // class and that method does not take any template argument that will enable
  // the compiler to resolve it unambiguously.
  using asap::logging::Loggable<
      BaseLookupTask>::internal_log_do_not_use_read_comment;

  /// Get a string representing the task for debugging
  [[nodiscard]] auto Name() const -> std::string {
    return std::string("[")
        .append(task_name_)
        .append("/")
        .append(std::to_string(in_flight_requests_count_))
        .append("]");
  }

protected:
  template <typename TNodes>
  BaseLookupTask(Node::IdType key, TNodes initial_peers, std::string task_name)
      : key_{std::move(key)}, task_name_(std::move(task_name)) {
    for (auto peer : initial_peers) {
      AddCandidate(peer);
    }
  }

  /// Default
  ~BaseLookupTask() = default;

  void MarkCandidateAsValid(Node::IdType const &candidate_id) {
    auto candidate = FindCandidate(candidate_id);
    if (candidate == candidates_.end()) {
      return;
    }

    --in_flight_requests_count_;
    candidate->second.state_ = Candidate::STATE_RESPONDED;
    ASLOG(trace, "{} candidate {} marked as STATE_RESPONDED", this->Name(),
        candidate_id.ToHex());
  }

  void MarkCandidateAsInvalid(Node::IdType const &candidate_id) {
    auto candidate = FindCandidate(candidate_id);
    if (candidate == candidates_.end()) {
      return;
    }

    --in_flight_requests_count_;
    candidate->second.state_ = Candidate::STATE_TIMED_OUT;
    ASLOG(trace, "{} candidate {} marked as STATE_TIMED_OUT", this->Name(),
        candidate_id.ToHex());
  }

  auto SelectUnContactedCandidates(std::size_t max_count) -> std::vector<Node> {
    std::vector<Node> selection;

    // Iterate over all candidates until we picked
    // candidates_max_count not-contacted candidates.
    for (auto candidate = candidates_.begin(), last = candidates_.end();
         candidate != last && in_flight_requests_count_ < max_count;
         ++candidate) {
      if (candidate->second.state_ == Candidate::STATE_UNKNOWN) {
        candidate->second.state_ = Candidate::STATE_CONTACTED;
        ++in_flight_requests_count_;
        selection.push_back(candidate->second.peer_);
      }
    }

    ASLOG(trace, "{} selected {} new fresh (not contacted) candidate",
        this->Name(), selection.size());
    for (auto &peer : selection) {
      ASLOG(trace, " -> {}", peer.ToString());
    }
    return selection;
  }

  auto GetValidCandidates(std::size_t max_count) -> std::vector<Node> {
    std::vector<Node> selection;

    // Iterate over all candidates until we picked
    // candidates_max_count responsive candidates.
    for (auto candidate = candidates_.begin(), last = candidates_.end();
         candidate != last && selection.size() < max_count; ++candidate) {
      if (candidate->second.state_ == Candidate::STATE_RESPONDED) {
        selection.push_back(candidate->second.peer_);
      }
    }

    ASLOG(trace, "{} selected {} valid (responded) candidate", this->Name(),
        selection.size());
    for (auto &peer : selection) {
      ASLOG(trace, " -> {}", peer.ToString());
    }
    return selection;
  }

  template <typename TPeers> void AddCandidates(TPeers const &peers) {
    for (auto const &peer : peers) {
      AddCandidate(peer);
    }
  }

  [[nodiscard]] auto AllRequestsCompleted() const -> bool {
    ASLOG(debug, "{} checking if all tasks completed, in-flight={}",
        this->Name(), in_flight_requests_count_);
    return in_flight_requests_count_ == 0;
  }

  [[nodiscard]] auto Key() const -> Node::IdType const & {
    return key_;
  }

private:
  ///
  struct Candidate final {
    Node peer_;
    enum {
      STATE_UNKNOWN,
      STATE_CONTACTED,
      STATE_RESPONDED,
      STATE_TIMED_OUT,
    } state_;
  };

  ///
  using CandidatesCollection = std::map<Node::IdType, Candidate>;

  void AddCandidate(Node const &peer) {
    auto const dist = Distance(peer.Id(), key_);
    Candidate const candidate{peer, Candidate::STATE_UNKNOWN};
    candidates_.emplace(dist, candidate);
  }

  auto FindCandidate(Node::IdType const &candidate_id)
      -> CandidatesCollection::iterator {
    auto const dist = Distance(candidate_id, key_);
    return candidates_.find(dist);
  }

  Node::IdType key_;
  std::size_t in_flight_requests_count_{};
  CandidatesCollection candidates_;
  std::string task_name_;
};

inline auto operator<<(std::ostream &out, BaseLookupTask const &task)
    -> std::ostream & {
  out << task.Name();
  return out;
}

} // namespace blocxxi::p2p::kademlia::detail
