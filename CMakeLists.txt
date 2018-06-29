#        Copyright The Authors 2018.
#    Distributed under the 3-Clause BSD License.
#    (See accompanying file LICENSE or copy at
#   https://opensource.org/licenses/BSD-3-Clause)

# We require 3.0 for modern, target-based CMake.
# We require 3.1 for the use of CXX_STANDARD in our targets.
# We require 3.6 for CXX_CLANG_TIDY
# We require 3.10 for CMAKE_CXX_CPPCHECK
if(ENABLE_CPPCHECK)
    cmake_minimum_required(VERSION 3.10)
else()
    cmake_minimum_required(VERSION 3.6)
endif()
project(blocxxi VERSION 0.9.4 LANGUAGES CXX C)

# The folder that will be used when generating project files for IDEs
# For example, Visual Studio supports folders.
# Must set the GLOBAL property USE_FOLDERS to ON for this to work.
set(BLOXI_IDE_FOLDER Blocxxi)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# List of directories specifying a search path for CMake modules to be loaded by
# the the include() or find_package() commands before checking the default modules
# that come with CMake.
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/common/cmake)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(ExternalProject)
find_package(Git REQUIRED)
include(GNUInstallDirs)
include(BuildHelpers)
include(DownloadProject)
include(FindDoxygen)

# detect operating system and host processor
message(STATUS "== We are on a ${CMAKE_SYSTEM_NAME} system")
message(STATUS "== The host processor is ${CMAKE_HOST_SYSTEM_PROCESSOR}")
message(STATUS "== CMAKE_CXX_COMPILER_ID is ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "== CMAKE_C_COMPILER_ID is ${CMAKE_C_COMPILER_ID}")

# The default build type provided by CMake is to include no compiler flags for
# optimization. For some projects you may want to set a default build type so
# that you do not have to remember to set it.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message("Setting build type to 'RelWithDebInfo' as none was specified.")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
            "MinSizeRel" "RelWithDebInfo")
endif()


# When wanting to specify a particular C++ version, the recommended way to do
# this with CMake 3.1 and later is to use the CXX_STANDARD, CXX_STANDARD_REQUIRED
# and CXX_EXTENSIONS target properties, or their variable equivalents to specify
# target defaults. Full details can be found at:
# (https://crascit.com/2015/03/28/enabling-cxx11-in-cmake/)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# config options
if (MSVC)
    # /wd4005  macro-redefinition
    # /wd4068  unknown pragma
    # /wd4244  conversion from 'type1' to 'type2'
    # /wd4267  conversion from 'size_t' to 'type2'
    # /wd4800  force value to bool 'true' or 'false' (performance warning)
    add_compile_options(/W3 /WX /wd4005 /wd4068 /wd4244 /wd4267 /wd4800)
    add_definitions(/DNOMINMAX /DWIN32_LEAN_AND_MEAN=1 /D_CRT_SECURE_NO_WARNINGS /D_SCL_SECURE_NO_WARNINGS /D_WIN32_WINNT=0x0501)
else ()
    add_compile_options(-Wall -pedantic -Wextra)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-depth=1024" CACHE STRING "" FORCE)
    endif()
endif ()

# Include our own module path. This makes #include "x.h"
# work in project subfolders to include the main directory headers.
include_directories(${CMAKE_CURRENT_SOURCE_DIR})


# This defines the basic input (src) and output (lib & include) directories.
set(SRCROOT "${CODEROOT}/src")
set(SRCINCLUDEROOT "${CODEROOT}/include")
set(DOXTOSRCDIR "../src")


# Google Test integration into the build
option(BLOXI_USE_GOOGLETEST_HEAD
        "If ON, we will download HEAD from googletest at config time." OFF)

option(BLOXI_RUN_TESTS "If ON, tests will be run." OFF)

set(BLOXI_RUN_TESTS ON)
if (${BLOXI_RUN_TESTS})
    # enable CTest.  This will set BUILD_TESTING to ON unless otherwise specified
    # on the command line
    include(CTest)
    enable_testing()
    include(External_googletest)
endif ()

if( CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT )
    message(STATUS "Setting default CMAKE_INSTALL_PREFIX path to ${CMAKE_BINARY_DIR}/install")
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE STRING "The path to use for make install" FORCE)
endif()


# CryptoPP integration
SET(CRYPTOPP_BUILD_SHARED OFF CACHE BOOL "Build shared library")
SET(CRYPTOPP_BUILD_TESTING OFF CACHE BOOL "Build library tests")
include(External_cryptopp)

add_subdirectory(third_party)

# ------------------------------------------------------------------------------
# Clang Tidy
# ------------------------------------------------------------------------------

if(ENABLE_CLANG_TIDY)

    find_program(
        CLANG_TIDY_EXE
        NAMES "clang-tidy" "clang-tidy-4.0"
        DOC "Path to clang-tidy executable")

    if(NOT CLANG_TIDY_EXE)
        message(FATAL_ERROR "unable to locate clang-tidy")
        macro(set_tidy_target_properties TARGET_NAME)
        endmacro()

    else ()

        list(APPEND CLANG_TIDY_BIN_ARGS
            -header-filter=.*
            -checks=*,-clang-analyzer-alpha.*
        )

        macro(set_tidy_target_properties TARGET_NAME)
            set_target_properties(
                ${TARGET_NAME} PROPERTIES
                CXX_CLANG_TIDY ${CLANG_TIDY_EXE} ${RUN_CLANG_TIDY_BIN_ARGS})
        endmacro()

    endif ()

else()
    macro(set_tidy_target_properties TARGET_NAME)
    endmacro()
endif()


# ------------------------------------------------------------------------------
# CppCheck
# ------------------------------------------------------------------------------

if(ENABLE_CPPCHECK)

    list(APPEND CPPCHECK_CMAKE_ARGS
        "-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}"
    )

    ExternalProject_Add(
        cppcheck
        GIT_REPOSITORY      https://github.com/danmar/cppcheck.git
        GIT_TAG             1.84
        GIT_SHALLOW         1
        CMAKE_ARGS          ${CPPCHECK_CMAKE_ARGS}
        PREFIX              ${CMAKE_BINARY_DIR}/external/cppcheck/prefix
        TMP_DIR             ${CMAKE_BINARY_DIR}/external/cppcheck/tmp
        STAMP_DIR           ${CMAKE_BINARY_DIR}/external/cppcheck/stamp
        DOWNLOAD_DIR        ${CMAKE_BINARY_DIR}/external/cppcheck/download
        SOURCE_DIR          ${CMAKE_BINARY_DIR}/external/cppcheck/src
        BINARY_DIR          ${CMAKE_BINARY_DIR}/external/cppcheck/build
    )
    set(CMAKE_CXX_CPPCHECK "${CMAKE_BINARY_DIR}/bin/cppcheck")
    macro(set_cppcheck_command)
        list(APPEND CMAKE_CXX_CPPCHECK
            "--enable=all"
            "--language=c++"
            "--std=c++14"
            "--error-exitcode=1"
            "--inline-suppr"
            "--suppress=*:*_build*"
            "--suppress=*:*third_party*"
            "--suppress=unmatchedSuppression:*"
        )
    endmacro()

else()
    macro(set_cppcheck_command)
    endmacro()
endif()


# ------------------------------------------------------------------------------
# Google Sanitizers
# ------------------------------------------------------------------------------

if(ENABLE_ASAN)
    set(ASAN_COMPILER_FLAGS "-g -O1 -fuse-ld=gold -fno-omit-frame-pointer -fsanitize=address -fsanitize=leak")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ASAN_COMPILER_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ASAN_COMPILER_FLAGS}")
endif()

if(ENABLE_USAN)
    set(USAN_COMPILER_FLAGS "-fuse-ld=gold -fsanitize=undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${USAN_COMPILER_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${USAN_COMPILER_FLAGS}")
endif()

if(ENABLE_TSAN)
    set(TSAN_COMPILER_FLAGS "-fuse-ld=gold -fsanitize=thread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TSAN_COMPILER_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TSAN_COMPILER_FLAGS}")
endif()

# ------------------------------------------------------------------------------
# Valgrind
# ------------------------------------------------------------------------------

find_program( MEMORYCHECK_COMMAND valgrind )
set(MEMORYCHECK_COMMAND_OPTIONS "${MEMORYCHECK_COMMAND_OPTIONS} --leak-check=full")
set(MEMORYCHECK_COMMAND_OPTIONS "${MEMORYCHECK_COMMAND_OPTIONS} --track-fds=yes")
set(MEMORYCHECK_COMMAND_OPTIONS "${MEMORYCHECK_COMMAND_OPTIONS} --trace-children=yes")
set(MEMORYCHECK_COMMAND_OPTIONS "${MEMORYCHECK_COMMAND_OPTIONS} --error-exitcode=1")


# Doxygen related macros and commands:
set(DOXROOT "${CMAKE_BINARY_DIR}/dox")

macro(configure_doxyfile TARGET_NAME TARGET_TITLE TARGET_BRIEF TARGET_INPUT_PATH)
    if (EXISTS "${CMAKE_SOURCE_DIR}/Doxyfile.in")
        set(DOXY_TARGET_OUTPUT_DIR "${TARGET_NAME}")
        set(DOXY_TARGET_ROOT_DIR "") # ${DOXTOSRCDIR} - set(DOXTOSRCDIR "../src")
        set(DOXY_TARGET_NAME "${TARGET_NAME}")
        set(DOXY_TARGET_TITLE "${TARGET_TITLE}")
        set(DOXY_TARGET_BRIEF "${TARGET_BRIEF}")
        set(DOXY_TARGET_INPUT_PATH "${TARGET_INPUT_PATH}")
        set(DOXY_TARGET_FILE_VERSION_FILTER "\"${PRINT_FILE_DATE_COMMAND_STR} \"")
        if (NOT EXISTS "${DOXROOT}/${DOXY_TARGET_OUTPUT_DIR}")
            file(MAKE_DIRECTORY "${DOXROOT}/${DOXY_TARGET_OUTPUT_DIR}")
        endif ()
        configure_file("${CMAKE_SOURCE_DIR}/Doxyfile.in" "${DOXROOT}/${TARGET_NAME}_Doxyfile" @ONLY)
        message(STATUS "== Copy Doxyfile.in --> '${DOXROOT}/${TARGET_NAME}_Doxyfile'")
    else ()
        message(STATUS "WARNING : The '${CMAKE_SOURCE_DIR}/Doxyfile.in' file does not exist!")
    endif ()
endmacro(configure_doxyfile)

if (DOXYGEN_FOUND)
    message(STATUS "Doxygen package was found.")
    add_custom_target(dox)
    macro(add_doxygen_target TARGET_NAME)
        add_custom_target(${TARGET_NAME}_dox
                ${CMAKE_COMMAND} -E remove -f "${TARGET_NAME}_Doxyfile.out"
                COMMAND ${CMAKE_COMMAND} -E copy "${TARGET_NAME}_Doxyfile" "${TARGET_NAME}_Doxyfile.out"
                COMMAND ${DOXYGEN_EXECUTABLE} "${TARGET_NAME}_Doxyfile.out"
                COMMAND ${CMAKE_COMMAND} -E remove -f "${TARGET_NAME}_Doxyfile.out"
                WORKING_DIRECTORY "${DOXROOT}" VERBATIM)
        add_dependencies(dox ${TARGET_NAME}_dox)
    endmacro()
	set_target_properties(dox PROPERTIES EXCLUDE_FROM_ALL TRUE)
else ()
    message(STATUS "WARNING : Doxygen package is not available on this system!")
    macro(add_doxygen_target TARGET_NAME)
    endmacro()
endif ()


# Project modules
add_subdirectory(common)
add_subdirectory(codec)
add_subdirectory(crypto)
add_subdirectory(nat)
add_subdirectory(p2p)
add_subdirectory(ndagent)
