# ===------------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause).
# SPDX-License-Identifier: BSD-3-Clause
# ===------------------------------------------------------------------------===#

set(my_name "p2p")
asap_push_module("${my_name}")

# ------------------------------------------------------------------------------
# Meta information about the this module
# ------------------------------------------------------------------------------

asap_declare_module(
  MODULE_NAME
  "${my_name}"
  DESCRIPTION
  "Kademlia DHT implementation"
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

# ==============================================================================
# Build instructions
# ==============================================================================

# ------------------------------------------------------------------------------
# Main module target
# ------------------------------------------------------------------------------

set(MODULE_TARGET_NAME "blocxxi_p2p")

asap_add_library(
  ${MODULE_TARGET_NAME}
  STATIC
  SHARED
  WARNING
  SOURCES
  # Headers
  "include/p2p/kademlia/error.h"
  "include/p2p/kademlia/boost_asio.h"
  "include/p2p/kademlia/parameters.h"
  "include/p2p/kademlia/session.h"
  "include/p2p/kademlia/detail/error_impl.h"
  "include/p2p/kademlia/node.h"
  "include/p2p/kademlia/buffer.h"
  "include/p2p/kademlia/channel.h"
  "include/p2p/kademlia/key.h"
  "include/p2p/kademlia/timer.h"
  "include/p2p/kademlia/kbucket.h"
  "include/p2p/kademlia/routing.h"
  "include/p2p/kademlia/engine.h"
  "include/p2p/kademlia/endpoint.h"
  "include/p2p/kademlia/message.h"
  "include/p2p/kademlia/message_serializer.h"
  "include/p2p/kademlia/response_dispatcher.h"
  "include/p2p/kademlia/network.h"
  "include/p2p/kademlia/detail/bootstrap_procedure.h"
  "include/p2p/kademlia/detail/find_node_task.h"
  "include/p2p/kademlia/detail/lookup_task.h"
  "include/p2p/kademlia/detail/find_value_task.h"
  "include/p2p/kademlia/detail/store_value_task.h"
  "include/p2p/kademlia/detail/ping_node_task.h"
  "include/p2p/kademlia/value_store.h"
  # Sources
  "src/kademlia/timer.cpp"
  "src/kademlia/message.cpp"
  "src/kademlia/message_serializer.cpp"
  "src/kademlia/error.cpp"
  "src/kademlia/error_impl.cpp"
  "src/kademlia/node.cpp"
  "src/kademlia/kbucket.cpp"
  "src/kademlia/routing.cpp")

target_link_libraries(
  ${MODULE_TARGET_NAME}
  PRIVATE asap::codec Boost::asio Boost::multiprecision
  PUBLIC asap::common asap::logging asap::crypto)

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
# Examples
# ------------------------------------------------------------------------------

if(ASAP_BUILD_EXAMPLES)
  add_subdirectory(examples)
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
  "\"P2P protocol implementation\""
  BRIEF
  "\"Kademlia based DHT implementation for blocxxi\""
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

asap_pop_module("${my_name}")
