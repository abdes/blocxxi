# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

# ------------------------------------------------------------------------------
# Meta information about the this module
# ------------------------------------------------------------------------------

nova_module_declare(
  MODULE_NAME "Nova.Base"
  DESCRIPTION "Common utilities and classes used across the codebase"
  WITHOUT_VERSION_H
)
nova_push_module("${META_MODULE_NAME}")

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------

find_package(fmt CONFIG REQUIRED)

# ==============================================================================
# Build instructions
# ==============================================================================

# ------------------------------------------------------------------------------
# Main module target
# ------------------------------------------------------------------------------

add_library(${META_MODULE_TARGET})
add_library(${META_MODULE_TARGET_ALIAS} ALIAS ${META_MODULE_TARGET})
set_target_properties(
  ${META_MODULE_TARGET}
  PROPERTIES
    DEFINE_SYMBOL
      "NOVA_BASE_EXPORTS"
    OUTPUT_NAME
      $<IF:$<BOOL:${BUILD_SHARED_LIBS}>,${META_MODULE_NAME},${META_MODULE_NAME}-static>
    VERSION
      ${META_VERSION}
    SOVERSION
      ${META_VERSION_MAJOR}
    PROJECT_LABEL
      ${META_MODULE_NAME}
)

set(
  NOVA_BASE_HEADERS
  "api_export.h"
  "AlwaysFalse.h"
  "Compilers.h"
  "Concepts.h"
  "Crtp.h"
  "Detail/LogWithAdapters.h"
  "Detail/NamedType_impl.h"
  "Detail/NamedType_skills.h"
  "Endian.h"
  "EnumIndexedArray.h"
  "Finally.h"
  "Hash.h"
  "Logging.h"
  "Macros.h"
  "NamedType.h"
  "NoInline.h"
  "NoStd.h"
  "ObserverPtr.h"
  "Platforms.h"
  "Resource.h"
  "ResourceHandle.h"
  "ResourceTable.h"
  "Result.h"
  "ReturnAddress.h"
  "ScopeGuard.h"
  "Sha256.h"
  "StateMachine.h"
  "StaticVector.h"
  "StringTypes.h"
  "StringUtils.h"
  "TypeList.h"
  "Types/Geometry.h"
  "Types/Overload.h"
  "Unreachable.h"
  "Uuid.h"
  "VariantHelpers.h"
  "Windows/ComError.h"
  "Windows/Exceptions.h"
)
set(
  NOVA_BASE_PRIVATE_SOURCES
  "Detail/loguru.cpp"
  "Sha256.cpp"
  "Uuid.cpp"
  "Windows/ComError.cpp"
  "Windows/Exceptions.cpp"
)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  list(
    APPEND
    NOVA_BASE_PRIVATE_SOURCES
    "Windows/ComError.cpp"
    "Windows/Exceptions.cpp"
  )
  list(
    APPEND
    NOVA_BASE_HEADERS
    "Windows/ComError.h"
    "Windows/Exceptions.h"
  )
endif()

target_sources(
  ${META_MODULE_TARGET}
  PRIVATE
    ${NOVA_BASE_PRIVATE_SOURCES}
    ${NOVA_BASE_HEADERS}
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${NOVA_SOURCE_DIR}
    FILES ${NOVA_BASE_HEADERS}
)

arrange_target_files_for_ide(${META_MODULE_TARGET})

target_compile_features(${META_MODULE_TARGET} PUBLIC cxx_std_20)
target_compile_options(${META_MODULE_TARGET} PUBLIC ${NOVA_COMMON_CXX_FLAGS})
target_compile_definitions(
  ${META_MODULE_TARGET}
  PUBLIC
    $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:NOVA_BASE_STATIC>
)

target_compile_definitions(
  ${META_MODULE_TARGET}
  PUBLIC
    "LOGURU_USE_FMTLIB=1"
  PRIVATE
    $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
)

target_link_libraries(${META_MODULE_TARGET} PUBLIC fmt::fmt-header-only)

# ------------------------------------------------------------------------------
# Tests
# ------------------------------------------------------------------------------

if(NOVA_BUILD_TESTS)
  add_subdirectory("Test")
endif()

# ------------------------------------------------------------------------------
# Examples
# ------------------------------------------------------------------------------

if(NOVA_BUILD_EXAMPLES)
  add_subdirectory("Examples")
endif()

# ------------------------------------------------------------------------------
# API Documentation
# ------------------------------------------------------------------------------

if(NOVA_BUILD_DOCS AND NOVA_WITH_DOXYGEN)
  nova_with_doxygen(
    MODULE_NAME ${META_MODULE_TARGET}
    VERSION ${META_VERSION}
    TITLE "\"Base\""
    BRIEF "\"Common utilities and classes used across the codebase\""

    INPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}"
  )
endif()

# ==============================================================================
# Deployment instructions
# ==============================================================================

nova_module_install(
  EXPORT ${META_MODULE_NAMESPACE}
  TARGETS
    ${META_MODULE_TARGET}
)

nova_pop_module("${META_MODULE_NAME}")
