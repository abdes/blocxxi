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
  using Loggable<BaseLookupTask>::internal_log_do_not_use_read_comment;

  /// Get a string representing the task for debugging
  [[nodiscard]] std::string Name() const {
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
    for (const auto &peer : initial_peers) {
      AddCandidate(peer);
    }
  }

  /// Default
  ~BaseLookupTask() = default;

  void MarkCandidateAsValid(const Node::IdType &candidate_id) {
    const auto candidate = FindCandidate(candidate_id);
    if (candidate == candidates_.end()) {
      return;
    }

    --in_flight_requests_count_;
    candidate->second.state_ = Candidate::STATE_RESPONDED;
    ASLOG(trace, "{} candidate {} marked as STATE_RESPONDED", this->Name(),
        candidate_id.ToHex());
  }

  void MarkCandidateAsInvalid(const Node::IdType &candidate_id) {
    const auto candidate = FindCandidate(candidate_id);
    if (candidate == candidates_.end()) {
      return;
    }

    --in_flight_requests_count_;
    candidate->second.state_ = Candidate::STATE_TIMED_OUT;
    ASLOG(trace, "{} candidate {} marked as STATE_TIMED_OUT", this->Name(),
        candidate_id.ToHex());
  }

  std::vector<Node> SelectUnContactedCandidates(std::size_t max_count) {
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

  std::vector<Node> GetValidCandidates(std::size_t max_count) {
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

  template <typename TPeers> void AddCandidates(const TPeers &peers) {
    for (const auto &peer : peers) {
      AddCandidate(peer);
    }
  }

  [[nodiscard]] bool AllRequestsCompleted() const {
    ASLOG(debug, "{} checking if all tasks completed, in-flight={}",
        this->Name(), in_flight_requests_count_);
    return in_flight_requests_count_ == 0;
  }

  [[nodiscard]] Node::IdType const &Key() const {
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

  void AddCandidate(const Node &peer) {
    const auto dist = Distance(peer.Id(), key_);
    const Candidate candidate{peer, Candidate::STATE_UNKNOWN};
    candidates_.emplace(dist, candidate);
  }

  CandidatesCollection::iterator FindCandidate(
      const Node::IdType &candidate_id) {
    const auto dist = Distance(candidate_id, key_);
    return candidates_.find(dist);
  }

  Node::IdType key_;
  std::size_t in_flight_requests_count_{};
  CandidatesCollection candidates_;
  std::string task_name_;
};

inline std::ostream &operator<<(std::ostream &out, const BaseLookupTask &task) {
  out << task.Name();
  return out;
}

} // namespace blocxxi::p2p::kademlia::detail
