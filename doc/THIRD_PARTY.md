











## Google Test Approach (https://raw.githubusercontent.com/google/googletest/master/googletest/README.md)
#### Incorporating Into An Existing CMake Project ####

If you want to use gtest in a project which already uses CMake, then a
more robust and flexible approach is to build gtest as part of that
project directly. This is done by making the GoogleTest source code
available to the main build and adding it using CMake's
`add_subdirectory()` command. This has the significant advantage that
the same compiler and linker settings are used between gtest and the
rest of your project, so issues associated with using incompatible
libraries (eg debug/release), etc. are avoided. This is particularly
useful on Windows. Making GoogleTest's source code available to the
main build can be done a few different ways:

* Download the GoogleTest source code manually and place it at a
  known location. This is the least flexible approach and can make
  it more difficult to use with continuous integration systems, etc.
* Embed the GoogleTest source code as a direct copy in the main
  project's source tree. This is often the simplest approach, but is
  also the hardest to keep up to date. Some organizations may not
  permit this method.
* Add GoogleTest as a git submodule or equivalent. This may not
  always be possible or appropriate. Git submodules, for example,
  have their own set of advantages and drawbacks.
* Use CMake to download GoogleTest as part of the build's configure
  step. This is just a little more complex, but doesn't have the
  limitations of the other methods.

The last of the above methods is implemented with a small piece
of CMake code in a separate file (e.g. `CMakeLists.txt.in`) which
is copied to the build area and then invoked as a sub-build
_during the CMake stage_. That directory is then pulled into the
main build with `add_subdirectory()`. For example:

New file `CMakeLists.txt.in`:

    cmake_minimum_required(VERSION 2.8.2)
 
    project(googletest-download NONE)
 
    include(ExternalProject)
    ExternalProject_Add(googletest
      GIT_REPOSITORY    https://github.com/google/googletest.git
      GIT_TAG           master
      SOURCE_DIR        "${CMAKE_BINARY_DIR}/googletest-src"
      BINARY_DIR        "${CMAKE_BINARY_DIR}/googletest-build"
      CONFIGURE_COMMAND ""
      BUILD_COMMAND     ""
      INSTALL_COMMAND   ""
      TEST_COMMAND      ""
    )
    
Existing build's `CMakeLists.txt`:

    # Download and unpack googletest at configure time
    configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
    execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
      RESULT_VARIABLE result
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
    if(result)
      message(FATAL_ERROR "CMake step for googletest failed: ${result}")
    endif()
    execute_process(COMMAND ${CMAKE_COMMAND} --build .
      RESULT_VARIABLE result
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
    if(result)
      message(FATAL_ERROR "Build step for googletest failed: ${result}")
    endif()

    # Prevent overriding the parent project's compiler/linker
    # settings on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    
    # Add googletest directly to our build. This defines
    # the gtest and gtest_main targets.
    add_subdirectory(${CMAKE_BINARY_DIR}/googletest-src
                     ${CMAKE_BINARY_DIR}/googletest-build
                     EXCLUDE_FROM_ALL)

    # The gtest/gtest_main targets carry header search path
    # dependencies automatically when using CMake 2.8.11 or
    # later. Otherwise we have to add them here ourselves.
    if (CMAKE_VERSION VERSION_LESS 2.8.11)
      include_directories("${gtest_SOURCE_DIR}/include")
    endif()

    # Now simply link against gtest or gtest_main as needed. Eg
    add_executable(example example.cpp)
    target_link_libraries(example gtest_main)
    add_test(NAME example_test COMMAND example)

Note that this approach requires CMake 2.8.2 or later due to
its use of the `ExternalProject_Add()` command. The above
technique is discussed in more detail in 
[this separate article](http://crascit.com/2015/07/25/cmake-gtest/)
which also contains a link to a fully generalized implementation
of the technique.

