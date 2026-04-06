# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

cmake_minimum_required(VERSION 3.29)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
if(EXISTS "${CMAKE_BINARY_DIR}/generators")
  list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/generators")
endif()
# If this project is not the master project, prioritize the master project
# cmake include dir if it exists.
if(
  NOT
    CMAKE_CURRENT_SOURCE_DIR
      STREQUAL
      CMAKE_SOURCE_DIR
  AND
    EXISTS
      "${CMAKE_SOURCE_DIR}/cmake"
)
  list(PREPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
endif()
# Include the log helpers module early so we can use early for logging the
# project hierarchy
include(LogHelpers)

set(CMAKE_DEBUG_POSTFIX "-d")

# ------------------------------------------------------------------------------
# Project description and (meta) information
# ------------------------------------------------------------------------------

# Get git revision
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)
string(SUBSTRING "${GIT_SHA1}" 0 12 GIT_REV)
if(NOT GIT_SHA1)
  set(GIT_REV "0")
endif()

# Meta information about the project
include(VersionHelpers)
# gersemi: off
set(META_PROJECT_NAME        "Blocxxi")
set(META_PROJECT_DESCRIPTION "Blocxxi on nova foundation")
set(META_AUTHOR_ORGANIZATION "The Authors")
set(META_GITHUB_REPO         "https://github.com/abdes/blocxxi")
set(META_AUTHOR_DOMAIN       "https://github.com/abdes")
set(META_AUTHOR_MAINTAINER   "Abdessattar Sassi")
nova_version_read() # from `VERSION` file
set(META_VERSION_REVISION    "${GIT_REV}")
set(META_VERSION             "${META_VERSION_MAJOR}.${META_VERSION_MINOR}.${META_VERSION_PATCH}")
set(META_NAME_VERSION        "${META_PROJECT_NAME} v${META_VERSION} (${META_VERSION_REVISION})")
# gersemi: on

string(MAKE_C_IDENTIFIER ${META_PROJECT_NAME} META_PROJECT_ID)
string(TOUPPER ${META_PROJECT_ID} META_PROJECT_ID)
string(TOLOWER ${META_PROJECT_ID} META_PROJECT_ID_LOWER)

# Determine if this is built as a subproject  or if it is the master project.
if(NOT DEFINED ${META_PROJECT_ID}_IS_MASTER_PROJECT)
  set(${META_PROJECT_ID}_IS_MASTER_PROJECT OFF)
  if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(${META_PROJECT_ID}_IS_MASTER_PROJECT ON)
  endif()
endif()

# Push the project on the hierarchy stack after we have determined if it is a
# master or a sub-project.
nova_push_project(${META_PROJECT_NAME})

# ------------------------------------------------------------------------------
# Project Declaration
# ------------------------------------------------------------------------------

# Generate folders for IDE targets (e.g., VisualStudio solutions)
set_property(
  GLOBAL
  PROPERTY
    USE_FOLDERS
      ON
)
set(IDE_FOLDER "${META_PROJECT_NAME}")

# Silence redundant toolchain warnings (often caused by IDEs passing the
# toolchain manually when it is already defined in the CMake preset).
if(CMAKE_TOOLCHAIN_FILE)
endif()

# Declare project
project(
  ${META_PROJECT_NAME}
  VERSION ${META_VERSION}
  DESCRIPTION ${META_PROJECT_DESCRIPTION}
  HOMEPAGE_URL ${META_GITHUB_REPO}
  LANGUAGES
    C
    CXX
)

# Work around an MSBuild parallel project-reference failure mode observed with
# /m>1 (returns exit code 1 with no compiler/linker errors). Keep top-level
# parallelism enabled, but force referenced projects to build sequentially.
if(CMAKE_GENERATOR MATCHES "^Visual Studio")
  if(NOT CMAKE_VS_GLOBALS MATCHES "(^|;)BuildInParallel=")
    if(CMAKE_VS_GLOBALS)
      set(CMAKE_VS_GLOBALS "${CMAKE_VS_GLOBALS};BuildInParallel=false")
    else()
      set(CMAKE_VS_GLOBALS "BuildInParallel=false")
    endif()
  endif()
endif()

# The default build type provided by CMake is to include no compiler flags for
# optimization. For some projects you may want to set a default build type so
# that you do not have to remember to set it.

# TODO: check for multi-config
# get_property(IS_MULTICONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
# if (NOT IS_MULTICONFIG AND NOT DEFINED CMAKE_BUILD_TYPE)
#     set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build.")
# endif()

get_property(IS_MULTICONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT IS_MULTICONFIG)
  if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(
      CMAKE_BUILD_TYPE
      Debug
      CACHE STRING
      "Choose the type of build."
      FORCE
    )
  endif()
  set_property(
    CACHE
      CMAKE_BUILD_TYPE
    PROPERTY
      STRINGS
        "Debug"
        "Release"
        "RelWithDebInfo"
  )
endif()

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Set the source tree root
set(NOVA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

# ------------------------------------------------------------------------------
# Project configuration options
# ------------------------------------------------------------------------------

# Project options
# gersemi: off
set(OPTIONS_LIST
  BUILD_SHARED_LIBS      "Build shared instead of static libraries." OFF
  NOVA_BUILD_TESTS       "Setup target to build and run tests." ON
  NOVA_BUILD_EXAMPLES    "Setup target to build the examples." ON
  NOVA_BUILD_DOCS        "Setup target to build the documentation." ON
  NOVA_WITH_ASAN         "Instrument code with address sanitizer" OFF
  NOVA_WITH_COVERAGE     "Instrument code to measure coverage" OFF
  # NOVA_WITH_UBSAN        "Instrument code with undefined behavior sanitizer" OFF
  # NOVA_WITH_TSAN         "Instrument code with thread sanitizer" OFF
  # NOVA_WITH_VALGRIND     "Instrument code with valgrind profilers" OFF
  NOVA_WITH_DOXYGEN      "Create targets for doxygen api docs" OFF
  # NOVA_WITH_SPHINX       "Create targets for sphinx docs" OFF
  # NOVA_WITH_CLANG_FORMAT "Create targets for clang-format" ON
  # NOVA_WITH_CLANG_TIDY   "Create targets for clang-tidy" ON
  NOVA_USE_CCACHE        "Enable caching of compiled artifacts using ccache" OFF
)
# gersemi: on
# Process each option

message(STATUS "PROJECT OPTIONS:")
list(LENGTH OPTIONS_LIST OPTIONS_LIST_length)
math(EXPR OPTIONS_LIST_length "${OPTIONS_LIST_length}-1")
foreach(index RANGE 0 ${OPTIONS_LIST_length} 3)
  list(GET OPTIONS_LIST ${index} OPTION_NAME)
  math(EXPR index "${index}+1")
  list(GET OPTIONS_LIST ${index} OPTION_DESCRIPTION)
  math(EXPR index "${index}+1")
  list(GET OPTIONS_LIST ${index} OPTION_DEFAULT)

  if(DEFINED CACHE{${OPTION_NAME}})
    set(${OPTION_NAME} "$CACHE{${OPTION_NAME}}")
    message(STATUS ". ${OPTION_NAME}: ${${OPTION_NAME}} - cache")
  elseif(DEFINED ${OPTION_NAME})
    set(${OPTION_NAME} "${${OPTION_NAME}}" CACHE BOOL ${OPTION_DESCRIPTION})
    message(STATUS ". ${OPTION_NAME}: ${${OPTION_NAME}} - toolchain")
  else()
    set(${OPTION_NAME} ${OPTION_DEFAULT} CACHE BOOL ${OPTION_DESCRIPTION})
    message(STATUS ". ${OPTION_NAME}: ${${OPTION_NAME}} - default")
  endif()
endforeach()

# ------------------------------------------------------------------------------
# Installation Setup
# ------------------------------------------------------------------------------

if(NOT DEFINED NOVA_CONAN_DEPLOY_DIR)
  set(NOVA_CONAN_DEPLOY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/out/install")
endif()

if(NOVA_WITH_ASAN)
  set(_NOVA_INSTALL_SUBDIR "Asan")
else()
  get_property(_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if(_IS_MULTI_CONFIG)
    # For multi-config generators, CMAKE_BUILD_TYPE is not available.
    # Default to 'Debug' for the install prefix; users can override
    # at install time using --prefix if needed.
    set(_NOVA_INSTALL_SUBDIR "Debug")
  else()
    set(_NOVA_INSTALL_SUBDIR "${CMAKE_BUILD_TYPE}")
  endif()
endif()

if(NOT _NOVA_INSTALL_SUBDIR STREQUAL "")
  set(
    CMAKE_INSTALL_PREFIX
    "${NOVA_CONAN_DEPLOY_DIR}/${_NOVA_INSTALL_SUBDIR}"
    CACHE PATH
    "Install path prefix, prepended onto install directories."
    FORCE
  )
endif()

# ------------------------------------------------------------------------------
# Top level code generation
# ------------------------------------------------------------------------------

# Generate a clangd configuration file that points to the compilation database
# in the cmake build directory. We need to do this as the build directory is
# different for every preset and can be different as well when the user decides
# to build somewhere else. Currently we cannot configure this properly in vscode
# settings. See https://github.com/clangd/vscode-clangd/issues/48
#
# NOTE: We don't want this to be running when building under Visual Studio. It
# will create issues with clangd which is only used when we generate for Ninja.
if(CMAKE_GENERATOR MATCHES "^Ninja")
  configure_file(.clangd.in ${CMAKE_SOURCE_DIR}/.clangd @ONLY)
endif()

# ------------------------------------------------------------------------------
# Testing
# ------------------------------------------------------------------------------

if(NOVA_BUILD_TESTS)
  include(CTest)
endif()

# ------------------------------------------------------------------------------
# Documentation - doxygen, sphinx/breathe/exhale
# ------------------------------------------------------------------------------

if(NOVA_BUILD_DOCS)
  if(NOVA_WITH_DOXYGEN)
    # Doxygen
    set(DNOVA_BUILD_DIR "${CMAKE_BINARY_DIR}/dox")
    include(DoxGeneration)
  endif()
endif()

# ------------------------------------------------------------------------------
# Additional CMake modules
# ------------------------------------------------------------------------------

# Register general cmake commands
include(BuildHelpers)
#include(AsapTargets)

# Override the ${META_PROJECT_ID}_INSTALL option to ON/OFF to respectively force
# install/no-install behavior for this project. This is particularly useful when
# this project is used as a sub-project and the user publicly depends on it and
# wants to have a self-contained install.
include(Install)

# ------------------------------------------------------------------------------
# Compiler flags
# ------------------------------------------------------------------------------

if(
  NOT
    DEFINED
      CMAKE_CXX_VISIBILITY_PRESET
  AND
    NOT
      DEFINED
        CMAKE_VISIBILITY_INLINES_HIDDEN
)
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
endif()

set(NOVA_COMMON_CXX_FLAGS)
if(NOVA_WITH_ASAN)
  list(APPEND NOVA_COMMON_CXX_FLAGS "-fsanitize=address")
  if(
    CMAKE_CXX_COMPILER_ID
      STREQUAL
      "Clang"
    OR
      CMAKE_CXX_COMPILER_ID
        STREQUAL
        "AppleClang"
    OR
      CMAKE_CXX_COMPILER_ID
        STREQUAL
        "GNU"
  )
    add_link_options("-fsanitize=address")
  endif()
  if(MSVC)
    # ASAN on MSVC does not support incremental linking. We must ensure that
    # /INCREMENTAL:NO is set in the cache variables so that it's reflected
    # in the IDE and all link commands correctly.
    set(
      CMAKE_MSVC_INCREMENTAL_LINKING
      OFF
      CACHE BOOL
      "Disable incremental linking for ASAN"
      FORCE
    )
    set(
      CMAKE_MSVC_INCREMENTAL_LINKING_DEBUG
      OFF
      CACHE BOOL
      "Disable incremental linking for ASAN"
      FORCE
    )
    set(
      CMAKE_MSVC_INCREMENTAL_LINKING_MINSIZEREL
      OFF
      CACHE BOOL
      "Disable incremental linking for ASAN"
      FORCE
    )
    set(
      CMAKE_MSVC_INCREMENTAL_LINKING_RELEASE
      OFF
      CACHE BOOL
      "Disable incremental linking for ASAN"
      FORCE
    )
    set(
      CMAKE_MSVC_INCREMENTAL_LINKING_RELWITHDEBINFO
      OFF
      CACHE BOOL
      "Disable incremental linking for ASAN"
      FORCE
    )
    set_property(
      DIRECTORY
      PROPERTY
        MSVC_INCREMENTAL_LINKING
          OFF
    )
    set(
      CMAKE_EXE_LINKER_FLAGS_DEBUG
      "/debug /INCREMENTAL:NO"
      CACHE STRING
      "Forced ASAN linker flags"
      FORCE
    )
    set(
      CMAKE_EXE_LINKER_FLAGS_MINSIZEREL
      "/INCREMENTAL:NO"
      CACHE STRING
      "Forced ASAN linker flags"
      FORCE
    )
    set(
      CMAKE_EXE_LINKER_FLAGS_RELEASE
      "/INCREMENTAL:NO"
      CACHE STRING
      "Forced ASAN linker flags"
      FORCE
    )
    set(
      CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO
      "/debug /INCREMENTAL:NO"
      CACHE STRING
      "Forced ASAN linker flags"
      FORCE
    )
    set(
      CMAKE_MODULE_LINKER_FLAGS_DEBUG
      "/debug /INCREMENTAL:NO"
      CACHE STRING
      "Forced ASAN linker flags"
      FORCE
    )
    set(
      CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL
      "/INCREMENTAL:NO"
      CACHE STRING
      "Forced ASAN linker flags"
      FORCE
    )
    set(
      CMAKE_MODULE_LINKER_FLAGS_RELEASE
      "/INCREMENTAL:NO"
      CACHE STRING
      "Forced ASAN linker flags"
      FORCE
    )
    set(
      CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO
      "/debug /INCREMENTAL:NO"
      CACHE STRING
      "Forced ASAN linker flags"
      FORCE
    )
    set(
      CMAKE_SHARED_LINKER_FLAGS_DEBUG
      "/debug /INCREMENTAL:NO"
      CACHE STRING
      "Forced ASAN linker flags"
      FORCE
    )
    set(
      CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL
      "/INCREMENTAL:NO"
      CACHE STRING
      "Forced ASAN linker flags"
      FORCE
    )
    set(
      CMAKE_SHARED_LINKER_FLAGS_RELEASE
      "/INCREMENTAL:NO"
      CACHE STRING
      "Forced ASAN linker flags"
      FORCE
    )
    set(
      CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO
      "/debug /INCREMENTAL:NO"
      CACHE STRING
      "Forced ASAN linker flags"
      FORCE
    )

    add_link_options(
      /LARGEADDRESSAWARE
      /DEBUG
      /INCREMENTAL:NO # Ensure it is also added to the link command
    )
  endif()
endif()
# MSVC-specific sanitizer handling will be done at the end of this file to ensure
# it overrides any flags added by subdirectories or toolchains.
if(MSVC)
  # Enable C++ latest features
  list(
    APPEND
    NOVA_COMMON_CXX_FLAGS
    "/Zc:__cplusplus"
    "/Zc:preprocessor"
  )
  list(APPEND NOVA_COMMON_CXX_FLAGS "/utf-8") # Set character set to UTF-8
  # Blocxxi needs RTTI (notably for Crypto++ usage), so this project
  # intentionally overrides Nova's default no-RTTI policy at the top level.
  list(APPEND NOVA_COMMON_CXX_FLAGS "/GR") # Enable RTTI
  list(APPEND NOVA_COMMON_CXX_FLAGS "/W4") # Warning level 4
  list(APPEND NOVA_COMMON_CXX_FLAGS "/MP") # Multi-processor compilation
  list(APPEND NOVA_COMMON_CXX_FLAGS "/DNOMINMAX") # Always define NOMINMAX for MSVC
  #  list(APPEND NOVA_COMMON_CXX_FLAGS "/WX") # Treat warnings as errors
else()
  # Blocxxi needs RTTI (notably for Crypto++ usage), so this project
  # intentionally overrides Nova's default no-RTTI policy at the top level.
  list(APPEND NOVA_COMMON_CXX_FLAGS "-frtti") # Enable RTTI
  list(APPEND NOVA_COMMON_CXX_FLAGS "-fPIC")
  # Enable all warnings and treat them as errors
  list(
    APPEND
    NOVA_COMMON_CXX_FLAGS
    "-Wall"
    "-Wextra"
    "-Werror"
  )
endif()

# ---- Speedup build using ccache ----
include(cmake/FasterBuild.cmake)

# ------------------------------------------------------------------------------
# Print a summary of the configuration
# ------------------------------------------------------------------------------

# Detect and report C++ standard support via module
include(cmake/DetectCxxStandards.cmake)

# Small helper to print support for a standard using a prefix
function(_report_standard prefix std_name)
  string(TOUPPER ${prefix} _PREF)
  set(_supported_var "${_PREF}_${std_name}_SUPPORTED")
  set(_flag_var "${_PREF}_${std_name}_FLAG")
  if(${${_supported_var}})
    message(STATUS "     ${std_name}: supported (${${_flag_var}})")
  else()
    message(STATUS "     ${std_name}: NOT supported")
  endif()
endfunction()

# This will detect C++ standard support and set variables
# NOVA_CXX20_SUPPORTED, NOVA_CXX20_FLAG, NOVA_CXX23_SUPPORTED,
# NOVA_CXX23_FLAG when <compiler> supports the respective standard
detect_cxx_standards(OXYGEN)

message(STATUS "PROJECT `${META_NAME_VERSION}`")
message(STATUS "")
message(STATUS "  -- Directories --")
message(STATUS "     Source directory: ${CMAKE_SOURCE_DIR}")
message(STATUS "     Build directory: ${CMAKE_BINARY_DIR}")
message(STATUS "     Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "  -- General configuration --")
message(STATUS "     CMake version: ${CMAKE_VERSION}")

message(
  STATUS
  "     C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
)

_report_standard(OXYGEN CXX20)
_report_standard(OXYGEN CXX23)

message(STATUS "     Common Compiler flags: ${NOVA_COMMON_CXX_FLAGS}")
message(STATUS "     Common Exe Linker flags: ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "     Build system: ${CMAKE_GENERATOR}")
message(STATUS "")
message(STATUS "  -- Build configuration --")
message(STATUS "     Export compile commands: ${CMAKE_EXPORT_COMPILE_COMMANDS}")
message(STATUS "     Build shared libraries: ${BUILD_SHARED_LIBS}")
message(STATUS "     Build tests: ${NOVA_BUILD_TESTS}")
message(STATUS "     Build examples: ${NOVA_BUILD_EXAMPLES}")
message(STATUS "     Build documentation: ${NOVA_BUILD_DOCS}")
message(STATUS "     Address sanitizer: ${NOVA_WITH_ASAN}")
message(STATUS "     Code coverage: ${NOVA_WITH_COVERAGE}")
message(STATUS "     Doxygen: ${NOVA_WITH_DOXYGEN}")
# message(STATUS "     Undefined behavior sanitizer: ${NOVA_WITH_UBSAN}")
# message(STATUS "     Thread sanitizer: ${NOVA_WITH_TSAN}")
# message(STATUS "     Valgrind: ${NOVA_WITH_VALGRIND}")
# message(STATUS "     Sphinx: ${NOVA_WITH_SPHINX}")
# message(STATUS "     Clang format: ${NOVA_WITH_CLANG_FORMAT}")
# message(STATUS "     Clang tidy: ${NOVA_WITH_CLANG_TIDY}")
message(STATUS "     CCache: ${NOVA_USE_CCACHE}")
message(STATUS "")

# ------------------------------------------------------------------------------
# Project modules and Solution Virtual Folders
# ------------------------------------------------------------------------------

# Show the repo .github folder inside the IDE under a virtual folder named
# ".github".
file(GLOB_RECURSE _github_files "${CMAKE_SOURCE_DIR}/.github/*")

if(_github_files)
  source_group(
    TREE "${CMAKE_SOURCE_DIR}/.github"
    PREFIX ".github"
    FILES
      ${_github_files}
  )
  # Convenience target listing them (keeps them in IDE but not part of build)
  add_custom_target(GitHub SOURCES ${_github_files})
endif()

add_subdirectory("src")

if(NOVA_BUILD_EXAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Examples")
  add_subdirectory("Examples")
endif()

# ------------------------------------------------------------------------------
# Code analyzers: clang-tidy, cppcheck, valgrind, sanitizers, etc...
#
# Call after sub-modules have been added so that source code files can be
# properly collected for analysis.
# ------------------------------------------------------------------------------

nova_pop_project(${META_PROJECT_NAME})
