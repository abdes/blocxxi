
#if(TARGET absl::base)
#	message(STATUS "Found Target: absl::base")
#else()
#	message(FATAL_ERROR "Target absl::base not found")
#endif()

list(APPEND NDAGENT_SRC
        "src/KHR/khrplatform.h"
        "src/glad/glad.h"
        "src/imgui_impl_opengl3.h"
        "src/imgui_impl_opengl3.cpp"
        "src/imgui_impl_glfw.h"
        "src/imgui_impl_glfw.cpp"
        "src/glad.c"
        "src/main.cpp"
        )

# Needed for spdlog
find_package(Threads REQUIRED)

set(NDAGENT_LIBRARIES
        bloxi::p2p
        bloxi::nat
        spdlog
        Boost::program_options
        Boost::thread
        imgui
        glfw
        Threads::Threads
        )


list(APPEND NDAGENT_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        )

bloxi_executable(TARGET
        TARGET
        ndagent
        SOURCES
        ${NDAGENT_SRC}
        INCLUDE_DIRS
        ${NDAGENT_INCLUDE_DIRS}
        LIBRARIES
        ${NDAGENT_LIBRARIES}
)
set_tidy_target_properties(ndagent)

set_cppcheck_command()
