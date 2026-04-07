# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

nova_module_declare(
  MODULE_NAME "Blocxxi.P2P"
  DESCRIPTION "Kademlia DHT implementation"
  WITHOUT_VERSION_H
)
nova_push_module("${META_MODULE_NAME}")

find_package(asio REQUIRED CONFIG PATHS "${CMAKE_BINARY_DIR}/generators")

add_library(${META_MODULE_TARGET})
add_library(${META_MODULE_TARGET_ALIAS} ALIAS ${META_MODULE_TARGET})
set_target_properties(
  ${META_MODULE_TARGET}
  PROPERTIES
    DEFINE_SYMBOL
      "BLOCXXI_P2P_EXPORTS"
    OUTPUT_NAME
      $<IF:$<BOOL:${BUILD_SHARED_LIBS}>,${META_MODULE_NAME},${META_MODULE_NAME}-static>
    VERSION
      ${META_VERSION}
    SOVERSION
      ${META_VERSION_MAJOR}
    PROJECT_LABEL
      ${META_MODULE_NAME}
)

file(
  GLOB_RECURSE BLOCXXI_P2P_HEADERS
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/kademlia/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/event_dht.h
  ${CMAKE_CURRENT_SOURCE_DIR}/api_export.h
)
file(
  GLOB_RECURSE BLOCXXI_P2P_SOURCES
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/kademlia/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/event_dht.cpp
)
list(
  TRANSFORM BLOCXXI_P2P_HEADERS
  REPLACE
    "^${CMAKE_CURRENT_SOURCE_DIR}/"
    ""
)
list(
  TRANSFORM BLOCXXI_P2P_SOURCES
  REPLACE
    "^${CMAKE_CURRENT_SOURCE_DIR}/"
    ""
)

target_sources(
  ${META_MODULE_TARGET}
  PRIVATE
    ${BLOCXXI_P2P_SOURCES}
    ${BLOCXXI_P2P_HEADERS}
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${NOVA_SOURCE_DIR}
    FILES ${BLOCXXI_P2P_HEADERS}
)

arrange_target_files_for_ide(${META_MODULE_TARGET})

target_include_directories(
  ${META_MODULE_TARGET}
  PUBLIC
    $<BUILD_INTERFACE:${NOVA_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(${META_MODULE_TARGET} PUBLIC cxx_std_20)
target_compile_options(${META_MODULE_TARGET} PUBLIC ${NOVA_COMMON_CXX_FLAGS})
target_compile_definitions(
  ${META_MODULE_TARGET}
  PUBLIC
    $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:BLOCXXI_P2P_STATIC>
)
target_link_libraries(
  ${META_MODULE_TARGET}
  PUBLIC
    nova::base
    blocxxi::codec
    blocxxi::core
    blocxxi::crypto
    asio::asio
)

if(NOVA_BUILD_TESTS)
  add_subdirectory("Test")
endif()
if(NOVA_BUILD_EXAMPLES)
  add_subdirectory("Examples")
endif()

nova_module_install(
  EXPORT ${META_MODULE_NAMESPACE}
  TARGETS
    ${META_MODULE_TARGET}
)

nova_pop_module("${META_MODULE_NAME}")
