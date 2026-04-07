# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

nova_module_declare(
  MODULE_NAME "Blocxxi.Crypto"
  DESCRIPTION "Cryptographic utilities"
  WITHOUT_VERSION_H
)
nova_push_module("${META_MODULE_NAME}")

find_package(cryptopp REQUIRED CONFIG PATHS "${CMAKE_BINARY_DIR}/generators")
include(CheckCXXSourceCompiles)

if(MSVC)
  check_cxx_source_compiles(
    "#include <intrin.h>\n#pragma intrinsic(_BitScanReverse)\n#pragma intrinsic(_BitScanReverse64)\nint main(){ unsigned long x=5; unsigned long y; _BitScanReverse(&y, x); _BitScanReverse64(&x, y); return 0; }"
    BLOCXXI_HAS_BITSCAN_REVERSE
  )
else()
  check_cxx_source_compiles(
    "#include <cstdint>\nint main(){ unsigned int x=5; unsigned long long y = __builtin_clz(x); x = __builtin_clzll(y); return 0; }"
    BLOCXXI_HAS_BUILTIN_CLZ
  )
endif()

configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/Blocxxi/Crypto/config.h)

add_library(${META_MODULE_TARGET})
add_library(${META_MODULE_TARGET_ALIAS} ALIAS ${META_MODULE_TARGET})
set_target_properties(
  ${META_MODULE_TARGET}
  PROPERTIES
    DEFINE_SYMBOL
      "BLOCXXI_CRYPTO_EXPORTS"
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
    hash.h
    keypair.h
    signature.h
    random.h
    hash.cpp
    keypair.cpp
    signature.cpp
    random.cpp
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${NOVA_SOURCE_DIR}
    FILES api_export.h hash.h keypair.h signature.h random.h
)

arrange_target_files_for_ide(${META_MODULE_TARGET})

target_include_directories(
  ${META_MODULE_TARGET}
  PUBLIC
    $<BUILD_INTERFACE:${NOVA_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(${META_MODULE_TARGET} PUBLIC cxx_std_20)
target_compile_options(${META_MODULE_TARGET} PUBLIC ${NOVA_COMMON_CXX_FLAGS})
target_compile_definitions(
  ${META_MODULE_TARGET}
  PUBLIC
    $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:BLOCXXI_CRYPTO_STATIC>
)
target_link_libraries(
  ${META_MODULE_TARGET}
  PUBLIC
    nova::base
    blocxxi::codec
    cryptopp::cryptopp
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
