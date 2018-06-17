//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <p2p/kademlia/message_serializer.h>

namespace blocxxi {
namespace p2p {
namespace kademlia {

MessageSerializer::MessageSerializer(Node::IdType const &my_id)
    : my_id_(my_id) {}

Header MessageSerializer::MakeHeader(Header::MessageType const &type,
                                     blocxxi::crypto::Hash160 const &token) {
  return Header{Header::Version::V1, type, my_id_, token};
}

Buffer MessageSerializer::Serialize(Header::MessageType const &type,
                                    blocxxi::crypto::Hash160 const &token) {
  auto const header = MakeHeader(type, token);

  Buffer buffer;
  blocxxi::p2p::kademlia::Serialize(header, buffer);

  return buffer;
}

}  // namespace kademlia
}  // namespace p2p
}  // namespace blocxxi
