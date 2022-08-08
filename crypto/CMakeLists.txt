# ===------------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause).
# SPDX-License-Identifier: BSD-3-Clause
# ===------------------------------------------------------------------------===#

# ------------------------------------------------------------------------------
# Meta information about the this module
# ------------------------------------------------------------------------------

asap_declare_module(
  MODULE_NAME
  "crypto"
  DESCRIPTION
  "Cryptography utilities"
  GITHUB_REPO
  "https://github.com/abdes/blocxxi"
  AUTHOR_MAINTAINER
  "Abdessattar Sassi"
  VERSION_MAJOR
  "1"
  VERSION_MINOR
  "0"
  VERSION_PATCH
  "0")
# ------------------------------------------------------------------------------
# Code generation
# ------------------------------------------------------------------------------

if(MSVC)
  check_cxx_source_compiles(
      "#include <intrin.h>
      #pragma intrinsic(_BitScanReverse)
      #pragma intrinsic(_BitScanReverse64)
      int main () {
          unsigned long x = 5;
          unsigned long y;
          _BitScanReverse(&y, x);
          _BitScanReverse64(&x, y);
          return 0;
      }"
      BLOCXXI_HAS_BITSCAN_REVERSE)
else()
  check_cxx_source_compiles(
      "#include <cstdint>
       int main () {
          unsigned int x = 5;
          unsigned long long y = __builtin_clz(x);
          x = __builtin_clzll(y);
          return 0;
      }"
      BLOCXXI_HAS_BUILTIN_CLZ)
endif()

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/include/crypto/config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/include/crypto/config.h)

# ==============================================================================
# Build instructions
# ==============================================================================

# ------------------------------------------------------------------------------
# Main module target
# ------------------------------------------------------------------------------

set(MODULE_TARGET_NAME "${META_PROJECT_NAME}_${META_MODULE_NAME}")

asap_add_library(
  ${MODULE_TARGET_NAME}
  STATIC
  SHARED
  WARNING
  SOURCES

  # Headers
  "include/crypto/hash.h"
  "include/crypto/keypair.h"
  "include/crypto/random.h"

  # Sources
  "src/hash.cpp"
  "src/keypair.cpp"
  "src/random.cpp"
  )

target_link_libraries(
  ${MODULE_TARGET_NAME}
  PRIVATE cryptopp-static Boost::endian
  PUBLIC asap::common asap::codec GSL
  )

target_include_directories(
  ${MODULE_TARGET_NAME}
  PUBLIC $<INSTALL_INTERFACE:include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(${MODULE_TARGET_NAME} PUBLIC cxx_std_17)

# logging comes directly from asap and is usually kept as is. It is more
# convenient to simply use it as asap::logging
add_library(asap::${META_MODULE_NAME} ALIAS ${MODULE_TARGET_NAME})

# Generate module config files for cmake and pkgconfig
asap_create_module_config_files()

# ------------------------------------------------------------------------------
# Tests
# ------------------------------------------------------------------------------
if(ASAP_BUILD_TESTS)
  asap_add_sanitizers(${MODULE_TARGET_NAME})
  add_subdirectory(test)
endif()

# ------------------------------------------------------------------------------
# API Documentation
# ------------------------------------------------------------------------------
asap_with_doxygen(
  MODULE_NAME
  ${MODULE_TARGET_NAME}
  VERSION
  ${META_MODULE_VERSION}
  TITLE
  "\"Crypto utilities\""
  BRIEF
  "\"Crypto utilities and basic types used for hashing, digests, encryption, etc..\""
  INPUT_PATH
  "${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include")

asap_with_sphinx(${MODULE_TARGET_NAME})

# ==============================================================================
# Deployment instructions
# ==============================================================================
if(${META_PROJECT_ID}_INSTALL)
  set(TARGETS_EXPORT_NAME "${MODULE_TARGET_NAME}Targets")
  set(runtime "${MODULE_TARGET_NAME}_runtime")
  set(dev "${MODULE_TARGET_NAME}_dev")

  # Library
  install(
    TARGETS ${MODULE_TARGET_NAME}
    EXPORT "${TARGETS_EXPORT_NAME}"
    COMPONENT dev
    RUNTIME DESTINATION ${ASAP_INSTALL_BIN} COMPONENT ${runtime}
    LIBRARY DESTINATION ${ASAP_INSTALL_SHARED} COMPONENT ${runtime}
    ARCHIVE DESTINATION ${ASAP_INSTALL_LIB} COMPONENT ${dev})

  # Header files
  install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/${META_MODULE_NAME}
    DESTINATION ${ASAP_INSTALL_INCLUDE}
    COMPONENT ${dev}
    FILES_MATCHING
    PATTERN "*.h")

  # Generated header files
  install(
    DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/${META_MODULE_NAME}
    DESTINATION ${ASAP_INSTALL_INCLUDE}
    COMPONENT ${dev})

  # Target config
  install(
    EXPORT ${TARGETS_EXPORT_NAME}
    NAMESPACE ${META_PROJECT_NAME}::
    DESTINATION ${ASAP_INSTALL_CMAKE}/${META_MODULE_NAME}
    COMPONENT ${dev})

  # Package configuration files
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_TARGET_NAME}Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_TARGET_NAME}ConfigVersion.cmake
    DESTINATION ${ASAP_INSTALL_CMAKE}/${META_MODULE_NAME})

  # Docs
  if(EXISTS ${SPHINX_BUILD_DIR}/${MODULE_TARGET_NAME})
    install(
      DIRECTORY ${SPHINX_BUILD_DIR}/${MODULE_TARGET_NAME}
      DESTINATION ${ASAP_INSTALL_DOC}
      COMPONENT ${MODULE_TARGET_NAME}_docs)
  endif()
endif()
