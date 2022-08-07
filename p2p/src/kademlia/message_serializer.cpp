//        Copyright The Authors 2018.
//    Distributed under the 3-Clause BSD License.
//    (See accompanying file LICENSE or copy at
//   https://opensource.org/licenses/BSD-3-Clause)

#include <p2p/kademlia/message_serializer.h>

namespace blocxxi::p2p::kademlia {

MessageSerializer::MessageSerializer(Node::IdType my_id)
    : my_id_(std::move(my_id)) {
}

auto MessageSerializer::MakeHeader(Header::MessageType const &type,
    blocxxi::crypto::Hash160 const &token) -> Header {
  return Header{Header::Version::V1, type, my_id_, token};
}

auto MessageSerializer::Serialize(Header::MessageType const &type,
    blocxxi::crypto::Hash160 const &token) -> Buffer {
  auto const header = MakeHeader(type, token);

  Buffer buffer;
  blocxxi::p2p::kademlia::Serialize(header, buffer);

  return buffer;
}

} // namespace blocxxi::p2p::kademlia
