# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause).
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

message(STATUS "${ASAP_LOG_PROJECT_HIERARCHY} >> third-party modules")

# Call `asap_add_package` to add third party packages using CPM, while
# automatically adding/removing them to the project hierarchy stack.
#
# Refer to https://github.com/cpm-cmake/CPM.cmake for the full usage.
#
# Example:
# ~~~
# asap_add_package(
#   NAME
#   magic_enum
#   GIT_TAG
#   master
#   GITHUB_REPOSITORY
#   abdes/magic_enum
#   OPTIONS
#   "MAGIC_ENUM_OPT_INSTALL ON")
# ~~~

# ------------------------------------------------------------------------------
# GSL
# ------------------------------------------------------------------------------
set(GSL_INSTALL
    ON
    CACHE BOOL FORCE "Generate and install GSL target")
asap_add_package(NAME gsl GIT_TAG main GITHUB_REPOSITORY microsoft/GSL)

# ------------------------------------------------------------------------------
# magic_enum
# ------------------------------------------------------------------------------
asap_add_package(
  NAME
  magic_enum
  GIT_TAG
  master
  GITHUB_REPOSITORY
  abdes/magic_enum
  OPTIONS
  "MAGIC_ENUM_OPT_INSTALL ON")

# ------------------------------------------------------------------------------
# Crypto++
# ------------------------------------------------------------------------------
asap_add_package(
  NAME
  cryptopp-cmake
  GIT_TAG
  master
  GITHUB_REPOSITORY
  abdes/cryptopp-cmake
  OPTIONS
  "CRYPTOPP_BUILD_TESTING OFF"
  "CRYPTOPP_INSTALL ON")
if(cryptopp-cmake_ADDED)
  # Because cryptopp generates so many g++ warnings (that we treat as errors),
  # just consider its include files like system includes so our own code does
  # not fail and we keep the -Werror for g++. Do this for both targets, the
  # static and the shared lib, depending on which one is used.
  target_include_directories(
    cryptopp SYSTEM PUBLIC $<BUILD_INTERFACE:${CRYPTOPP_PROJECT_DIR}>
                           $<INSTALL_INTERFACE:include>)
endif()

# ------------------------------------------------------------------------------
# Boost libs
# ------------------------------------------------------------------------------
foreach(
  boost_lib
  algorithm
  align
  array
  asio
  assert
  bind
  chrono
  concept_check
  config
  container
  container_hash
  context
  conversion
  core
  coroutine
  date_time
  describe
  detail
  dynamic_bitset
  endian
  exception
  function
  function_types
  functional
  fusion
  integer
  intrusive
  io
  iterator
  lexical_cast
  math
  move
  mp11
  mpl
  multiprecision
  numeric_conversion
  optional
  pool
  predef
  preprocessor
  random
  range
  ratio
  rational
  regex
  smart_ptr
  static_assert
  system
  throw_exception
  tokenizer
  tuple
  type_index
  type_traits
  typeof
  unordered
  utility
  variant2
  winapi)
  asap_add_package(
    NAME
    boost_${boost_lib}
    GIT_TAG
    "boost-1.81.0"
    GITHUB_REPOSITORY
    boostorg/${boost_lib}
    OPTIONS
    "BUILD_TESTING OFF")
endforeach()

# ------------------------------------------------------------------------------
# miniupnpc
# ------------------------------------------------------------------------------
asap_push_module(miniupnp)
cpmaddpackage(
  NAME
  miniupnp
  GIT_TAG
  master
  GITHUB_REPOSITORY
  miniupnp/miniupnp
  DOWNLOAD_ONLY
  YES)
if(miniupnp_ADDED)
  function(add_miniupnpc_subdir)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(UPNPC_BUILD_SHARED FALSE)
    set(UPNPC_BUILD_TESTS FALSE)
    set(UPNPC_BUILD_SAMPLE FALSE)
    set(UPNPC_NO_INSTALL TRUE)
    add_subdirectory(${MINIUPNPC_CMAKE_DIR} ${MINIUPNPC_BUILD_DIR})
  endfunction()

  set(MINIUPNPC_CMAKE_DIR ${miniupnp_SOURCE_DIR}/miniupnpc)
  set(MINIUPNPC_BUILD_DIR ${CMAKE_BINARY_DIR}/third_party_deps/miniupnpc-build)

  # Let cmake do the rest using the cryptopp-cmake CMakeLists.txt
  add_miniupnpc_subdir()

  # target_compile_definitions(libminiupnpc-static PRIVATE DEBUG)
endif()
asap_pop_module(miniupnp)

# ------------------------------------------------------------------------------
# asap::common
# ------------------------------------------------------------------------------
asap_add_package(
  NAME
  asap_common
  GIT_TAG
  master
  GITHUB_REPOSITORY
  asap-projects/asap-common
  OPTIONS
  "ASAP_COMMON_INSTALL TRUE"
  "ASAP_BUILD_TESTS OFF"
  "ASAP_BUILD_EXAMPLES OFF")

# ------------------------------------------------------------------------------
# asap::contract
# ------------------------------------------------------------------------------
asap_add_package(
  NAME
  asap_contract
  GIT_TAG
  master
  GITHUB_REPOSITORY
  asap-projects/asap-contract
  OPTIONS
  "ASAP_CONTRACT_INSTALL TRUE"
  "ASAP_BUILD_TESTS OFF"
  "ASAP_BUILD_EXAMPLES OFF")

# ------------------------------------------------------------------------------
# asap::logging
# ------------------------------------------------------------------------------
asap_add_package(
  NAME
  asap_logging
  GIT_TAG
  master
  GITHUB_REPOSITORY
  asap-projects/asap-logging
  OPTIONS
  "ASAP_LOGGING_INSTALL TRUE"
  "ASAP_BUILD_TESTS OFF"
  "ASAP_BUILD_EXAMPLES OFF")
