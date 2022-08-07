# ===------------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause).
# SPDX-License-Identifier: BSD-3-Clause
# ===------------------------------------------------------------------------===#

# ==============================================================================
# Build instructions
# ==============================================================================

set(MAIN_TEST_TARGET_NAME ${MODULE_TARGET_NAME}_test)

asap_add_test(
  ${MAIN_TEST_TARGET_NAME}
  UNIT_TEST
  VALGRIND_MEMCHECK
  SRCS
  "node_test.cpp"
  "kbucket_test.cpp"
  "routingtable_test.cpp"
  "channel_test.cpp"
  "network_test.cpp"
  "timer_test.cpp"
  "response_dispatcher_test.cpp"
  "main.cpp"
  LINK
  blocxxi::p2p
  Boost::asio
  Boost::multiprecision
  gtest
  gmock
  COMMENT
  "Blocxxi nat unit tests")

target_include_directories(${MAIN_TEST_TARGET_NAME} PRIVATE "../src")

gtest_discover_tests(${MAIN_TEST_TARGET_NAME})

# Add support for (optional) code quality tools
asap_add_sanitizers(${MAIN_TEST_TARGET_NAME})
swift_add_valgrind_massif(${MAIN_TEST_TARGET_NAME})
swift_add_valgrind_callgrind(${MAIN_TEST_TARGET_NAME})


# list(APPEND P2P_SIM_SRC
#   simulation.cpp
# )
# set(P2P_SIM_LIBRARIES asap::p2p spdlog Boost::program_options)
# asap_executable(TARGET
#   p2psim
#   SOURCES
#   ${P2P_SIM_SRC}
#   LIBRARIES
#   ${P2P_SIM_LIBRARIES}
# )
# set_tidy_target_properties(p2psim)
