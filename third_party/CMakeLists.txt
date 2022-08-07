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
#
# algorithm
# align
# array
# asio
# assert
# bind
# chrono
# concept_check
# config
# container
# container_hash
# context
# conversion
# core
# coroutine
# date_time
# detail
# dynamic_bitset
# endian
# exception
# function
# function_types
# fusion
# integer
# intrusive
# io
# iterator
# lexical_cast
# math
# move
# mp11
# mpl
# multiprecision
# numeric_conversion
# optional
# pool
# predef
# preprocessor
# random
# range
# ratio
# rational
# regex
# smart_ptr
# static_assert
# system
# throw_exception
# tokenizer
# tuple
# type_index
# type_traits
# typeof
# unordered
# utility
# variant2
# winapi
# ------------------------------------------------------------------------------
CPMAddPackage(
  NAME boost_algorithm
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/algorithm
)
CPMAddPackage(
  NAME boost_align
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/align
)
CPMAddPackage(
  NAME boost_array
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/array
)
CPMAddPackage(
  NAME boost_asio
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/asio
)
CPMAddPackage(
  NAME boost_assert
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/assert
)
CPMAddPackage(
  NAME boost_bind
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/bind
)
CPMAddPackage(
  NAME boost_chrono
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/chrono
)
CPMAddPackage(
  NAME boost_concept_check
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/concept_check
)
CPMAddPackage(
  NAME boost_config
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/config
)
CPMAddPackage(
  NAME boost_container
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/container
)
CPMAddPackage(
  NAME boost_container_hash
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/container_hash
)
CPMAddPackage(
  NAME boost_context
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/context
)
CPMAddPackage(
  NAME boost_conversion
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/conversion
)
CPMAddPackage(
  NAME boost_core
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/core
)
CPMAddPackage(
  NAME boost_coroutine
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/coroutine
)
CPMAddPackage(
  NAME boost_date_time
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/date_time
)
CPMAddPackage(
  NAME boost_detail
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/detail
)
CPMAddPackage(
  NAME boost_dynamic_bitset
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/dynamic_bitset
)
CPMAddPackage(
  NAME boost_endian
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/endian
)
CPMAddPackage(
  NAME boost_exception
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/exception
)
CPMAddPackage(
  NAME boost_function
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/function
)
CPMAddPackage(
  NAME boost_function_types
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/function_types
)
CPMAddPackage(
  NAME boost_fusion
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/fusion
)
CPMAddPackage(
  NAME boost_integer
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/integer
)
CPMAddPackage(
  NAME boost_intrusive
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/intrusive
)
CPMAddPackage(
  NAME boost_io
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/io
)
CPMAddPackage(
  NAME boost_iterator
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/iterator
)
CPMAddPackage(
  NAME boost_lexical_cast
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/lexical_cast
)
CPMAddPackage(
  NAME boost_math
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/math
)
CPMAddPackage(
  NAME boost_move
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/move
)
CPMAddPackage(
  NAME boost_mp11
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/mp11
)
CPMAddPackage(
  NAME boost_mpl
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/mpl
)
CPMAddPackage(
  NAME boost_multiprecision
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/multiprecision
)
CPMAddPackage(
  NAME boost_numeric_conversion
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/numeric_conversion
)
CPMAddPackage(
  NAME boost_optional
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/optional
)
CPMAddPackage(
  NAME boost_pool
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/pool
)
CPMAddPackage(
  NAME boost_predef
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/predef
)
CPMAddPackage(
  NAME boost_preprocessor
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/preprocessor
)
CPMAddPackage(
  NAME boost_random
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/random
)
CPMAddPackage(
  NAME boost_range
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/range
)
CPMAddPackage(
  NAME boost_ratio
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/ratio
)
CPMAddPackage(
  NAME boost_rational
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/rational
)
CPMAddPackage(
  NAME boost_regex
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/regex
)
CPMAddPackage(
  NAME boost_smart_ptr
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/smart_ptr
)
CPMAddPackage(
  NAME boost_static_assert
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/static_assert
)
CPMAddPackage(
  NAME boost_system
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/system
)
CPMAddPackage(
  NAME boost_throw_exception
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/throw_exception
)
CPMAddPackage(
  NAME boost_tokenizer
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/tokenizer
)
CPMAddPackage(
  NAME boost_tuple
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/tuple
)
CPMAddPackage(
  NAME boost_type_index
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/type_index
)
CPMAddPackage(
  NAME boost_type_traits
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/type_traits
)
CPMAddPackage(
  NAME boost_typeof
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/typeof
)
CPMAddPackage(
  NAME boost_unordered
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/unordered
)
CPMAddPackage(
  NAME boost_utility
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/utility
)
CPMAddPackage(
  NAME boost_variant2
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/variant2
)
CPMAddPackage(
  NAME boost_winapi
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/winapi
)

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
