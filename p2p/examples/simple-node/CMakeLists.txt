# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause).
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

# ==============================================================================
# Build instructions
# ==============================================================================

cpmaddpackage(
  NAME
  boost_algorithm
  GIT_TAG
  "boost-1.79.0"
  GITHUB_REPOSITORY
  boostorg/boost_algorithm
  OPTIONS
  "BUILD_TESTING OFF")

cpmaddpackage(
  NAME
  boost_any
  GIT_TAG
  "boost-1.79.0"
  GITHUB_REPOSITORY
  boostorg/any
  OPTIONS
  "BUILD_TESTING OFF")

cpmaddpackage(
  NAME
  boost_program_options
  GIT_TAG
  "boost-1.79.0"
  GITHUB_REPOSITORY
  boostorg/program_options
  OPTIONS
  "BUILD_TESTING OFF")

cpmaddpackage(
  NAME
  boost_atomic
  GIT_TAG
  "boost-1.79.0"
  GITHUB_REPOSITORY
  boostorg/atomic
  OPTIONS
  "BUILD_TESTING OFF")

cpmaddpackage(
  NAME
  boost_thread
  GIT_TAG
  "boost-1.79.0"
  GITHUB_REPOSITORY
  boostorg/thread
  OPTIONS
  "BUILD_TESTING OFF")

asap_add_executable(
  simple-node
  WARNING
  SOURCES
  "main.cpp"
  "../utils/console_runner.cpp"
  "../utils/console_runner.h"
  "../utils/runner_base.h")
target_link_libraries(
  simple-node
  PRIVATE asap::logging
          blocxxi::nat
          blocxxi::p2p
          Boost::algorithm
          Boost::program_options
          Boost::asio
          Boost::thread)
