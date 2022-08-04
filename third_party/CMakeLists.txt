# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause).
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

message("=> [third-party modules]")

# ------------------------------------------------------------------------------
# asap::common
# ------------------------------------------------------------------------------
set(ASAP_COMMON_INSTALL TRUE)
CPMAddPackage(
  NAME asap_common
  GIT_TAG master
  GITHUB_REPOSITORY asap-projects/asap-common
)

# ------------------------------------------------------------------------------
# GSL
# ------------------------------------------------------------------------------
set(GSL_INSTALL TRUE)
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
    set(BUILD_TESTING OFF)
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
CPMAddPackage(
  NAME boost_config
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/config
)
CPMAddPackage(
  NAME boost_core
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/core
)
CPMAddPackage(
  NAME boost_assert
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/assert
)
CPMAddPackage(
  NAME boost_static_assert
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/static_assert
)
CPMAddPackage(
  NAME boost_throw_exception
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/throw_exception
)
CPMAddPackage(
  NAME boost_type_traits
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/type_traits
)
CPMAddPackage(
  NAME boost_endian
  GIT_TAG "boost-1.79.0"
  GITHUB_REPOSITORY boostorg/endian
)
