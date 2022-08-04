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
  set(CRYPTOPP_CMAKE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cryptopp-cmake)
  set(CRYPTOPP_BUILD_DIR ${CMAKE_BINARY_DIR}/third_party_deps/cryptopp-build)

  set(cryptopp_DISPLAY_CMAKE_SUPPORT_WARNING FALSE)
  set(CRYPTOPP_PROJECT_DIR ${cryptopp_SOURCE_DIR} CACHE PATH "Crypto++ project directory")

  # # Copy files from cryptopp-cmake to the working sources dir
  # configure_file(
  #         ${CRYPTOPP_CMAKE_DIR}/CMakeLists.txt
  #         ${cryptopp_SOURCE_DIR}/CMakeLists.txt COPYONLY)
  # configure_file(
  #         ${CRYPTOPP_CMAKE_DIR}/cryptopp-config.cmake
  #         ${cryptopp_SOURCE_DIR}/cryptopp-config.cmake COPYONLY)

  # Let cmake do the rest using the cryptopp-cmake CMakeLists.txt
  add_subdirectory(
    ${CRYPTOPP_CMAKE_DIR}
    ${CRYPTOPP_BUILD_DIR}
  )

  # Add the include directory
  include_directories(SYSTEM ${cryptopp_SOURCE_DIR})
endif()
