# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

nova_module_declare(
  MODULE_NAME "Blocxxi.Chain"
  DESCRIPTION "Reusable blockchain kernel contracts and orchestrators"
  WITHOUT_VERSION_H
)
nova_push_module("${META_MODULE_NAME}")

add_library(${META_MODULE_TARGET})
add_library(${META_MODULE_TARGET_ALIAS} ALIAS ${META_MODULE_TARGET})
set_target_properties(
  ${META_MODULE_TARGET}
  PROPERTIES
    DEFINE_SYMBOL
      "BLOCXXI_CHAIN_EXPORTS"
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
    api_export.h
    kernel.h
    kernel.cpp
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${NOVA_SOURCE_DIR}
    FILES api_export.h kernel.h
)

arrange_target_files_for_ide(
  ${META_MODULE_TARGET}
  EXCLUDE_PATTERNS
    "^block_store\\.h$"
    "^chain_kernel\\.(h|cpp)$"
    "^fork_choice\\.h$"
    "^storage_ports\\.h$"
    "^validator\\.h$"
)

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
    $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:BLOCXXI_CHAIN_STATIC>
)
target_link_libraries(
  ${META_MODULE_TARGET}
  PUBLIC
    nova::base
    blocxxi::core
)

if(NOVA_BUILD_TESTS)
  add_subdirectory("Test")
endif()

nova_module_install(
  EXPORT ${META_MODULE_NAMESPACE}
  TARGETS
    ${META_MODULE_TARGET}
)

nova_pop_module("${META_MODULE_NAME}")
