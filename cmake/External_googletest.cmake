
## check targets
if (BUILD_TESTING)
    # Download googletest
    download_project(
            PROJ googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG master
            UPDATE_DISCONNECTED 1
    )

    # Prevent GoogleTest from overriding our compiler/linker options
    # when building with Visual Studio
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})

    # When using CMake 2.8.11 or later, header path dependencies
    # are automatically added to the gtest and gmock targets.
    # For earlier CMake versions, we have to explicitly add the
    # required directories to the header search path ourselves.
    if (CMAKE_VERSION VERSION_LESS 2.8.11)
        include_directories("${gtest_SOURCE_DIR}/include"
                "${gmock_SOURCE_DIR}/include")
    endif ()

    check_target(gtest)
    check_target(gtest_main)
    check_target(gmock)

    list(APPEND BLOXI_TEST_COMMON_LIBRARIES
            gtest_main
            gtest
            gmock
            ${CMAKE_THREAD_LIBS_INIT}
            )
endif ()
