
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
  src/ui/fonts/material_design_icons.cpp
  src/ui/fonts/material_design_icons.h
  #
  src/ui/abstract_application.h
  src/ui/application_base.h
  src/ui/application_base.cpp
  src/ui/application.h
  src/ui/application.cpp
  src/ui/style/theme.h
  src/ui/style/theme.cpp
  src/ui/log/sink.h
  src/ui/log/sink.cpp
  #
  src/ui/kademlia/debug_ui.h
  src/ui/kademlia/routing.cpp
  #
  src/imgui/imgui_dock.h
  src/imgui/imgui_dock.cpp
  #
  src/fsl.h
  src/fsl.cpp
  src/KHR/khrplatform.h
  src/glad/gl.h
  src/imgui/imgui_impl_opengl3.h
  src/imgui/imgui_impl_opengl3.cpp
  src/imgui/imgui_impl_glfw.h
  src/imgui/imgui_impl_glfw.cpp
  src/imgui/glad.c
  src/runner_base.h
  src/console_runner.h
  src/console_runner.cpp
  src/imgui_runner.h
  src/imgui_runner.cpp
  src/main.cpp
  )

# Needed for spdlog
find_package(Threads REQUIRED)

set(NDAGENT_LIBRARIES
  asap::p2p
  asap::nat
  yaml-cpp
  spdlog
  date_interface
  Boost::program_options
  Boost::thread
  Boost::filesystem
  imgui
  glfw
  Threads::Threads
  )


list(APPEND NDAGENT_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

asap_executable(TARGET
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
