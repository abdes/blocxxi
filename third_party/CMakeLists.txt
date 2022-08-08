# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause).
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

message("=> [third-party modules]")

# ------------------------------------------------------------------------------
# GSL
# ------------------------------------------------------------------------------
set(GSL_INSTALL ON CACHE BOOL FORCE "Generate and install GSL target")
CPMAddPackage(
  NAME gsl
  GIT_TAG main
  GITHUB_REPOSITORY microsoft/GSL
)

# ------------------------------------------------------------------------------
# Crypto++
# ------------------------------------------------------------------------------
CPMAddPackage(
  NAME cryptopp
  GIT_TAG CRYPTOPP_8_6_0
  GITHUB_REPOSITORY weidai11/cryptopp
  DOWNLOAD_ONLY YES
)
if(cryptopp_ADDED)
  function(add_cryptopp_subdir)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_TESTING OFF)
    set(BUILD_SHARED OFF)
    add_subdirectory(
      ${CRYPTOPP_CMAKE_DIR}
      ${CRYPTOPP_BUILD_DIR}
    )
  endfunction()

  set(CRYPTOPP_CMAKE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cryptopp-cmake)
  set(CRYPTOPP_BUILD_DIR ${CMAKE_BINARY_DIR}/third_party_deps/cryptopp-build)

  set(cryptopp_DISPLAY_CMAKE_SUPPORT_WARNING FALSE)
  set(CRYPTOPP_PROJECT_DIR ${cryptopp_SOURCE_DIR} CACHE PATH "Crypto++ project directory")
  # Let cmake do the rest using the cryptopp-cmake CMakeLists.txt
  add_cryptopp_subdir()

  # Add the include directory
  include_directories(SYSTEM ${cryptopp_SOURCE_DIR})
endif()

# ------------------------------------------------------------------------------
# Boost libs
# ------------------------------------------------------------------------------
foreach(boost_lib
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
  detail
  dynamic_bitset
  endian
  exception
  function
  function_types
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
  winapi
)
  CPMAddPackage(
    NAME boost_${boost_lib}
    GIT_TAG "boost-1.79.0"
    GITHUB_REPOSITORY boostorg/${boost_lib}
    OPTIONS "BUILD_TESTING OFF"
  )
endforeach()

# ------------------------------------------------------------------------------
# miniupnpc
# ------------------------------------------------------------------------------
CPMAddPackage(
  NAME miniupnp
  GIT_TAG master
  GITHUB_REPOSITORY miniupnp/miniupnp
  DOWNLOAD_ONLY YES
)
if(miniupnp_ADDED)
  function(add_miniupnpc_subdir)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(UPNPC_BUILD_SHARED FALSE)
    set(UPNPC_BUILD_TESTS FALSE)
    set(UPNPC_BUILD_SAMPLE FALSE)
    set(UPNPC_NO_INSTALL TRUE)
    add_subdirectory(
      ${MINIUPNPC_CMAKE_DIR}
      ${MINIUPNPC_BUILD_DIR}
    )
  endfunction()

  set(MINIUPNPC_CMAKE_DIR ${miniupnp_SOURCE_DIR}/miniupnpc)
  set(MINIUPNPC_BUILD_DIR ${CMAKE_BINARY_DIR}/third_party_deps/miniupnpc-build)

  # Let cmake do the rest using the cryptopp-cmake CMakeLists.txt
  add_miniupnpc_subdir()

  # Add the include directory
  include_directories(SYSTEM ${MINIUPNPC_CMAKE_DIR}/include)
endif()

# ------------------------------------------------------------------------------
# asap::common
# ------------------------------------------------------------------------------
CPMAddPackage(
  NAME asap_common
  GIT_TAG master
  GITHUB_REPOSITORY asap-projects/asap-common
  OPTIONS "ASAP_COMMON_INSTALL TRUE"
)

# ------------------------------------------------------------------------------
# asap::logging
# ------------------------------------------------------------------------------
CPMAddPackage(
  NAME asap_logging
  GIT_TAG master
  GITHUB_REPOSITORY asap-projects/asap-logging
  OPTIONS "ASAP_LOGGING_INSTALL TRUE"
)
