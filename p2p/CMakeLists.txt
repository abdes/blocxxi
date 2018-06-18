
list(APPEND P2P_PUBLIC_HEADERS
  "include/p2p/kademlia/error.h"
  "include/p2p/kademlia/parameters.h"
  "include/p2p/kademlia/session.h"
  "include/p2p/kademlia/detail/error_impl.h"
  "include/p2p/kademlia/node.h"
  "include/p2p/kademlia/buffer.h"
  "include/p2p/kademlia/channel.h"
  "include/p2p/kademlia/key.h"
  "include/p2p/kademlia/timer.h"
  "include/p2p/kademlia/kbucket.h"
  "include/p2p/kademlia/routing.h"
  "include/p2p/kademlia/engine.h"
  "include/p2p/kademlia/endpoint.h"
  "include/p2p/kademlia/message.h"
  "include/p2p/kademlia/message_serializer.h"
  "include/p2p/kademlia/response_dispatcher.h"
  "include/p2p/kademlia/network.h"
  "include/p2p/kademlia/detail/bootstrap_procedure.h"
  "include/p2p/kademlia/detail/find_node_task.h"
  "include/p2p/kademlia/detail/lookup_task.h"
  "include/p2p/kademlia/detail/find_value_task.h"
  "include/p2p/kademlia/detail/store_value_task.h"
  "include/p2p/kademlia/detail/ping_node_task.h"
  "include/p2p/kademlia/value_store.h"
  )

list(APPEND P2P_SRC
  "src/kademlia/timer.cpp"
  "src/kademlia/message.cpp"
  "src/kademlia/message_serializer.cpp"
  "src/kademlia/error.cpp"
  "src/kademlia/error_impl.cpp"
  "src/kademlia/node.cpp"
  "src/kademlia/kbucket.cpp"
  "src/kademlia/routing.cpp"
  ${P2P_PUBLIC_HEADERS}
  )

list(APPEND P2P_PUBLIC_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CRYPTOPP_INCLUDE_DIRS}
  )

list(APPEND P2P_PRIVATE_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

set(P2P_PUBLIC_LIBRARIES
  bloxi::crypto
  bloxi::codec
  bloxi::common
  cryptopp-static
  Boost::boost
  Boost::system
  )

bloxi_library(
  TARGET bloxi_p2p
  SOURCES ${P2P_SRC}
  EXPORT_NAME p2p
  PUBLIC_INCLUDE_DIRS ${P2P_PUBLIC_INCLUDE_DIRS}
  PUBLIC_PRIVATE_DIRS ${P2P_PRIVATE_INCLUDE_DIRS}
  PUBLIC_LIBRARIES ${P2P_PUBLIC_LIBRARIES}
)
set_tidy_target_properties(bloxi_p2p)

add_subdirectory(test)

configure_doxyfile(P2PLib
  "\"P2P Module\""
  "\"A module implementing a P2P node using a DHT, including an implementation of Kademlia.\""
  "${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include")
add_doxygen_target(P2PLib)
