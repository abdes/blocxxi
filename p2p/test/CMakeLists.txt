
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

set(P2P_LIBRARIES asap::p2p gtest_main gtest gmock)

asap_test(
  TARGET
  p2p_test
  SOURCES
  ${P2P_TEST_SRC}
  PUBLIC_LIBRARIES
  ${P2P_LIBRARIES}
)
set_tidy_target_properties(p2p_test_bin)

#Needed for spdlog
find_package(Threads REQUIRED)

set(P2P_SIM_LIBRARIES asap::p2p spdlog Boost::program_options)

asap_executable(TARGET
  p2psim
  SOURCES
  ${P2P_SIM_SRC}
  LIBRARIES
  ${P2P_SIM_LIBRARIES}
)
set_tidy_target_properties(p2psim)
