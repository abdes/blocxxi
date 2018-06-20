

include(CMakeParseArguments)

function(set_common_compiler_flags TARGET)
  if (MSVC)
      # /wd4005  macro-redefinition
      # /wd4068  unknown pragma
      # /wd4244  conversion from 'type1' to 'type2'
      # /wd4267  conversion from 'size_t' to 'type2'
      # /wd4800  force value to bool 'true' or 'false' (performance warning)
      target_compile_options(${TARGET} PUBLIC /W3 /WX /wd4005 /wd4068 /wd4244 /wd4267 /wd4800)
      target_compile_definitions(${TARGET} PUBLIC /DNOMINMAX /DWIN32_LEAN_AND_MEAN=1 /D_CRT_SECURE_NO_WARNINGS /D_SCL_SECURE_NO_WARNINGS /D_WIN32_WINNT=0x0501)
  else ()
      target_compile_options(${TARGET} PUBLIC -Wall -pedantic -Wextra)
      if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
          target_compile_options(${TRAGET} PUBLIC -ftemplate-depth=1024)
      endif()
  endif ()
endfunction()

# create a library in the bloxi namespace
#
# parameters
# SOURCES : sources files for the library
# PUBLIC_LIBRARIES: targets and flags for linking phase
# PRIVATE_COMPILE_FLAGS: compile flags for the library. Will not be exported.
# EXPORT_NAME: export name for the bloxi:: target export
# TARGET: target name
#
# create a target associated to <NAME>
# libraries are installed under CMAKE_INSTALL_FULL_LIBDIR by default
#
function(bloxi_library)
  cmake_parse_arguments(BLOXI_LIB
    "DISABLE_INSTALL" # keep that in case we want to support installation one day
    "TARGET;EXPORT_NAME"
    "SOURCES;PUBLIC_LIBRARIES;PUBLIC_INCLUDE_DIRS;PRIVATE_COMPILE_FLAGS;PRIVATE_INCLUDE_DIRS"
    ${ARGN}
  )

  set(_NAME ${BLOXI_LIB_TARGET})
  string(TOUPPER ${_NAME} _UPPER_NAME)

  add_library(${_NAME} STATIC ${BLOXI_LIB_SOURCES})

  set_common_compiler_flags(${_NAME})
  target_compile_options(${_NAME} PRIVATE ${BLOXI_COMPILE_CXXFLAGS} ${BLOXI_LIB_PRIVATE_COMPILE_FLAGS})
  target_link_libraries(${_NAME} PUBLIC ${BLOXI_LIB_PUBLIC_LIBRARIES})
  target_include_directories(${_NAME}
    PUBLIC ${BLOXI_COMMON_INCLUDE_DIRS} ${BLOXI_LIB_PUBLIC_INCLUDE_DIRS}
    PRIVATE ${BLOXI_LIB_PRIVATE_INCLUDE_DIRS}
  )
  # Add all targets to a folder in the IDE for organization.
  set_property(TARGET ${_NAME} PROPERTY FOLDER ${BLOXI_IDE_FOLDER})

  if(BLOXI_LIB_EXPORT_NAME)
    add_library(bloxi::${BLOXI_LIB_EXPORT_NAME} ALIAS ${_NAME})
  endif()
endfunction()



#
# header only virtual target creation
#
function(bloxi_header_library)
  cmake_parse_arguments(BLOXI_HO_LIB
    "DISABLE_INSTALL"
    "EXPORT_NAME;TARGET"
    "PUBLIC_LIBRARIES;PRIVATE_COMPILE_FLAGS;PUBLIC_INCLUDE_DIRS;PRIVATE_INCLUDE_DIRS"
    ${ARGN}
  )

  set(_NAME ${BLOXI_HO_LIB_TARGET})

  set(__dummy_header_only_lib_file "${CMAKE_CURRENT_BINARY_DIR}/${_NAME}_header_only_dummy.cc")

  if(NOT EXISTS ${__dummy_header_only_lib_file})
    file(WRITE ${__dummy_header_only_lib_file}
      "/* generated file for header-only cmake target */

      namespace bloxi {

       // single meaningless symbol
       void ${_NAME}__header_fakesym() {}
      }  // namespace bloxi
      "
    )
  endif()


  add_library(${_NAME} ${__dummy_header_only_lib_file})
  set_common_compiler_flags(${_NAME})
  target_link_libraries(${_NAME} PUBLIC ${BLOXI_HO_LIB_PUBLIC_LIBRARIES})
  target_include_directories(${_NAME}
    PUBLIC ${BLOXI_COMMON_INCLUDE_DIRS} ${BLOXI_HO_LIB_PUBLIC_INCLUDE_DIRS}
    PRIVATE ${BLOXI_HO_LIB_PRIVATE_INCLUDE_DIRS}
  )

  # Add all targets to a folder in the IDE for organization.
  set_property(TARGET ${_NAME} PROPERTY FOLDER ${BLOXI_IDE_FOLDER})

  if(BLOXI_HO_LIB_EXPORT_NAME)
    add_library(bloxi::${BLOXI_HO_LIB_EXPORT_NAME} ALIAS ${_NAME})
  endif()

endfunction()


#
# 
#
function(bloxi_executable)

  cmake_parse_arguments(BLOXI_EXE
    "DISABLE_INSTALL"
    "TARGET"
    "SOURCES;LIBRARIES;PRIVATE_COMPILE_FLAGS;INCLUDE_DIRS"
    ${ARGN}
  )

    set(_NAME ${BLOXI_EXE_TARGET})

    add_executable(${_NAME} ${BLOXI_EXE_SOURCES})
    set_common_compiler_flags(${_NAME})
    target_compile_options(${_NAME} PRIVATE ${BLOXI_COMPILE_CXXFLAGS} ${BLOXI_TEST_PRIVATE_COMPILE_FLAGS})
    target_link_libraries(${_NAME} PUBLIC ${BLOXI_EXE_LIBRARIES})
    target_include_directories(${_NAME} PUBLIC ${BLOXI_COMMON_INCLUDE_DIRS} ${BLOXI_EXE_INCLUDE_DIRS}
	)

    # Add all targets to a folder in the IDE for organization.
    set_property(TARGET ${_NAME} PROPERTY FOLDER ${BLOXI_IDE_FOLDER})

endfunction()

#
# create a unit_test and add it to the executed test list
#
# parameters
# TARGET: target name prefix
# SOURCES: sources files for the tests
# PUBLIC_LIBRARIES: targets and flags for linking phase.
# PRIVATE_COMPILE_FLAGS: compile flags for the test. Will not be exported.
#
# create a target associated to <NAME>_bin
#
# all tests will be register for execution with add_test()
#
# test compilation and execution is disable when BUILD_TESTING=OFF
#
function(bloxi_test)

  cmake_parse_arguments(BLOXI_TEST
    ""
    "TARGET"
    "SOURCES;PUBLIC_LIBRARIES;PRIVATE_COMPILE_FLAGS;PUBLIC_INCLUDE_DIRS"
    ${ARGN}
  )


  if(BUILD_TESTING)

    set(_NAME ${BLOXI_TEST_TARGET})
    string(TOUPPER ${_NAME} _UPPER_NAME)

    add_executable(${_NAME}_bin ${BLOXI_TEST_SOURCES})
    set_common_compiler_flags(${_NAME}_bin)
    target_compile_options(${_NAME}_bin PRIVATE ${BLOXI_COMPILE_CXXFLAGS} ${BLOXI_TEST_PRIVATE_COMPILE_FLAGS})
    target_link_libraries(${_NAME}_bin PUBLIC ${BLOXI_TEST_PUBLIC_LIBRARIES} ${BLOXI_TEST_COMMON_LIBRARIES})
    target_include_directories(${_NAME}_bin
      PUBLIC ${BLOXI_COMMON_INCLUDE_DIRS} ${BLOXI_TEST_PUBLIC_INCLUDE_DIRS} ${GMOCK_INCLUDE_DIRS} ${GTEST_INCLUDE_DIRS}
    )

    # Add all targets to a folder in the IDE for organization.
    set_property(TARGET ${_NAME}_bin PROPERTY FOLDER ${BLOXI_IDE_FOLDER})

    add_test(${_NAME} ${_NAME}_bin)
  endif(BUILD_TESTING)

endfunction()
