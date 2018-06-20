
list(APPEND CODEC_PUBLIC_HEADERS
        "include/codec/base16.h"
        )

list(APPEND CODEC_SRC
        "src/base16.cpp"
        ${CODEC_PUBLIC_HEADERS}
        )

list(APPEND CODEC_PUBLIC_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CRYPTOPP_INCLUDE_DIRS}
        )

set(CODEC_PUBLIC_LIBRARIES gsl-lite)

bloxi_library(
        TARGET
        bloxi_codec
        SOURCES
        ${CODEC_SRC}
        EXPORT_NAME
        codec
        PUBLIC_INCLUDE_DIRS
        ${CODEC_PUBLIC_INCLUDE_DIRS}
        PUBLIC_LIBRARIES
        ${CODEC_PUBLIC_LIBRARIES}
)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        target_compile_options(bloxi_codec PRIVATE -ftemplate-depth=1024)
endif()

set_tidy_target_properties(bloxi_codec)

add_subdirectory(test)

set_cppcheck_command()

configure_doxyfile(CoDecLib 
                   "\"CoDec Module\"" 
                   "\"A module to provide encoding and decoding facilities such as hex etc.\"" 
                   "${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include")
add_doxygen_target(CoDecLib)
