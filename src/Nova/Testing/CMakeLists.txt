# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

# ------------------------------------------------------------------------------
# Meta information about the this module
# ------------------------------------------------------------------------------

nova_module_declare(
  MODULE_NAME "Nova.Testing"
  DESCRIPTION "Testing helpers for unit test code writing"
  WITHOUT_VERSION_H
)
nova_push_module("${META_MODULE_NAME}")

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------

find_package(GTest REQUIRED CONFIG)

# ==============================================================================
# Build instructions
# ==============================================================================

# ------------------------------------------------------------------------------
# Main module target - Always static
# ------------------------------------------------------------------------------

add_library(${META_MODULE_TARGET} STATIC)
add_library(${META_MODULE_TARGET_ALIAS} ALIAS ${META_MODULE_TARGET})
set_target_properties(
  ${META_MODULE_TARGET}
  PROPERTIES
    OUTPUT_NAME
      $<IF:$<BOOL:${BUILD_SHARED_LIBS}>,${META_MODULE_NAME},${META_MODULE_NAME}-static>
    VERSION
      ${META_VERSION}
    SOVERSION
      ${META_VERSION_MAJOR}
    PROJECT_LABEL
      ${META_MODULE_NAME}
)

target_sources(
  ${META_MODULE_TARGET}
  PRIVATE
    "gtest_main.cpp"
  INTERFACE
    FILE_SET HEADERS
    TYPE HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES "GTest.h" "ScopedLogCapture.h"
)

# Set include directories
target_include_directories(
  ${META_MODULE_TARGET}
  INTERFACE
    # Includes will have the following format: #include <Nova/OxCo/Logging.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
    $<INSTALL_INTERFACE:include>
)

set_target_properties(
  ${META_MODULE_TARGET}
  PROPERTIES
    FOLDER
      "Testing"
    OUTPUT_NAME
      "${META_MODULE_NAME}"
)

target_compile_features(${META_MODULE_TARGET} INTERFACE cxx_std_20)
target_compile_options(
  ${META_MODULE_TARGET}
  INTERFACE
    ${NOVA_COMMON_CXX_FLAGS}
    $<$<CXX_COMPILER_ID:MSVC>:/wd4702> # Unreachable code
)
target_link_libraries(
  ${META_MODULE_TARGET}
  PUBLIC
    nova::base
    GTest::gtest
    GTest::gmock
)

# ==============================================================================
# Deployment instructions
# ==============================================================================

# nova_module_install(
#   EXPORT ${META_MODULE_NAMESPACE}
#   TARGETS
#     ${META_MODULE_TARGET}
# )

nova_pop_module("${META_MODULE_NAME}")
