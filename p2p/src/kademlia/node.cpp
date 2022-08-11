//===----------------------------------------------------------------------===//
// Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
// copy at https://opensource.org/licenses/BSD-3-Clause).
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#include <p2p/kademlia/node.h>

namespace blocxxi::p2p::kademlia {

namespace {
// table of leading zero counts for bytes [0..255]
constexpr unsigned int lzcount[256]{
    // clang-format off
    8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // clang-format on
};
} // anonymous namespace

// Find the number of common prefix bits in the id of the nodes within the given
// iterator range.
inline auto SharedPrefixSize(const Node &a, const Node &b) -> unsigned int {
  auto lz = 0U;
  auto ida = a.Id();
  auto idb = b.Id();
  auto size = Node::IdType::Size();
  for (auto i = 0U; i < size; ++i) {
    auto x = ida[i] ^ idb[i];
    if (x == 0) {
      lz += 8;
    } else {
      lz += lzcount[x];
      break;
    }
  }
  return lz;
}

auto Node::LogDistanceTo(blocxxi::crypto::Hash160 const &hash) const -> size_t {
  auto dist = DistanceTo(hash);
  auto lzc = dist.LeadingZeroBits();
  return (Node::IdType::BitSize() - 1 - lzc);
}

auto Node::ToString() const -> std::string const & {
  // TODO(Abdessattar): use std::optional instead of a pointer
  if (url_) {
    return *url_;
  }

  url_ = new std::string();
  url_->append("knode://")
      .append(node_id_.ToHex())
      .append("@")
      .append(endpoint_.Address().to_string())
      .append(":")
      .append(std::to_string(static_cast<unsigned int>(endpoint_.Port())));
  return *url_;
}

auto Node::FromUrlString(const std::string &url) -> Node {
  // Validate the URL string and extract the different parts out of it
  auto pos = url.find("://");
  if (pos != std::string::npos) {
    auto start = 0U;
    auto url_type = url.substr(start, pos - start);
    if (url_type == "knode") {
      start = pos + 3;
      pos = url.find('@', start);
      if (pos != std::string::npos) {
        auto id = url.substr(start, pos - start);
        if (id.length() == 40) {
          start = pos + 1;
          pos = url.rfind(':');
          if (pos != std::string::npos) {
            auto address = url.substr(start, pos - start);
            auto port = url.substr(pos + 1);

            return {IdType::FromHex(id), address,
                static_cast<unsigned short>(std::stoi(port))};
          }
        }
      }
    }
  }
  throw std::invalid_argument("bad node url: " + url);
}

} // namespace blocxxi::p2p::kademlia
