
list(APPEND P2P_TEST_SRC
  node_test.cpp
  kbucket_test.cpp
  routingtable_test.cpp
  channel_test.cpp
  network_test.cpp
  timer_test.cpp
  response_dispatcher_test.cpp
)

list(APPEND P2P_SIM_SRC
  simulation.cpp
)

set(P2P_LIBRARIES bloxi::p2p)

bloxi_test(
  TARGET
  p2p_test
  SOURCES
  ${P2P_TEST_SRC}
  PUBLIC_LIBRARIES
  ${P2P_LIBRARIES}
)

#Needed for spdlog
find_package(Threads REQUIRED)

set(P2P_SIM_LIBRARIES bloxi::p2p spdlog Boost::program_options)

bloxi_executable(TARGET
  "p2psim"
  SOURCES
  ${P2P_SIM_SRC}
  LIBRARIES
  ${P2P_SIM_LIBRARIES}
)