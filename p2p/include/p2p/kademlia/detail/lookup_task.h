//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#ifndef BLOCXXI_P2P_KADEMLIA_LOOKUP_TASK_H_
#define BLOCXXI_P2P_KADEMLIA_LOOKUP_TASK_H_

#include <map>
#include <ostream>
#include <vector>

#include <common/logging.h>
#include <p2p/kademlia/node.h>

namespace blocxxi {
namespace p2p {
namespace kademlia {
namespace detail {

/*!
 *
 */
class BaseLookupTask
 : protected asap::logging::Loggable<asap::logging::Id::P2P_KADEMLIA> {
 public:
  /// Get a string representing the task for debugging
  std::string Name() const {
    return std::string("[")
        .append(task_name_)
        .append("/")
        .append(std::to_string(in_flight_requests_count_))
        .append("]");
  };

 protected:
  /*!
   *
   * @tparam Nodes
   * @param key
   * @param initial_peers
   */
  template <typename TNodes>
  BaseLookupTask(Node::IdType const &key, TNodes initial_peers,
                 std::string const &task_name)
      : key_{key},
        in_flight_requests_count_{0},
        candidates_{},
        task_name_(task_name) {
    for (auto peer : initial_peers) AddCandidate(peer);
  }

  /// Default
  virtual ~BaseLookupTask() = default;

  /*!
   *
   * @param candidate_id
   */
  void MarkCandidateAsValid(Node::IdType const &candidate_id) {
    auto candidate = FindCandidate(candidate_id);
    if (candidate == candidates_.end()) return;

    --in_flight_requests_count_;
    candidate->second.state_ = Candidate::STATE_RESPONDED;
    ASLOG(trace, "{} candidate {} marked as STATE_RESPONDED", this->Name(),
          candidate_id.ToHex());
  }

  /*!
   *
   * @param candidate_id
   */
  void MarkCandidateAsInvalid(Node::IdType const &candidate_id) {
    auto candidate = FindCandidate(candidate_id);
    if (candidate == candidates_.end()) return;

    --in_flight_requests_count_;
    candidate->second.state_ = Candidate::STATE_TIMED_OUT;
    ASLOG(trace, "{} candidate {} marked as STATE_TIMED_OUT", this->Name(),
          candidate_id.ToHex());
  }

  /*!
   *
   * @param max_count
   * @return
   */
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

  /*!
   *
   * @param max_count
   * @return
   */
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

  template <typename TPeers>
  void AddCandidates(TPeers const &peers) {
    for (auto const &peer : peers) {
      AddCandidate(peer);
    }
  }

  bool AllRequestsCompleted(void) const {
    ASLOG(debug, "{} checking if all tasks completed, in-flight={}",
          this->Name(), in_flight_requests_count_);
    return in_flight_requests_count_ == 0;
  }

  Node::IdType const &Key(void) const { return key_; }

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
    Candidate const c{peer, Candidate::STATE_UNKNOWN};
    candidates_.emplace(dist, c);
  }

  CandidatesCollection::iterator FindCandidate(
      Node::IdType const &candidate_id) {
    auto const dist = Distance(candidate_id, key_);
    return candidates_.find(dist);
  }

 private:
  ///
  Node::IdType key_;
  ///
  std::size_t in_flight_requests_count_;
  ///
  CandidatesCollection candidates_;
  ///
  std::string task_name_;
};

inline std::ostream &operator<<(std::ostream &out, BaseLookupTask const &task) {
  out << task.Name();
  return out;
}

}  // namespace detail
}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi

#endif  // BLOCXXI_P2P_KADEMLIA_LOOKUP_TASK_H_
