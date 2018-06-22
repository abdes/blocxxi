
list(APPEND NDAGENT_SRC
        #FONTS
        src/ui/fonts/fonts.h
        src/ui/fonts/Roboto-Black.cpp
        src/ui/fonts/Roboto-BlackItalic.cpp
        src/ui/fonts/Roboto-Bold.cpp
        src/ui/fonts/Roboto-BoldItalic.cpp
        src/ui/fonts/Roboto-Italic.cpp
        src/ui/fonts/Roboto-Light.cpp
        src/ui/fonts/Roboto-LightItalic.cpp
        src/ui/fonts/Roboto-Medium.cpp
        src/ui/fonts/Roboto-MediumItalic.cpp
        src/ui/fonts/Roboto-Regular.cpp
        src/ui/fonts/Roboto-Thin.cpp
        src/ui/fonts/Roboto-ThinItalic.cpp
        src/ui/fonts/Inconsolata-Regular.cpp
        src/ui/fonts/Inconsolata-Bold.cpp
        #
        src/ui/debug_ui.h
        src/ui/style/theme.h
        src/ui/style/theme.cpp
        src/ui/log/sink.h
        src/ui/log/sink.cpp
        src/ui/log/settings.cpp
	#
        src/KHR/khrplatform.h
        src/glad/gl.h
        src/imgui_impl_opengl3.h
        src/imgui_impl_opengl3.cpp
        src/imgui_impl_glfw.h
        src/imgui_impl_glfw.cpp
        src/glad.c
        src/main.cpp
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
