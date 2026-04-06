# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

# ==============================================================================
# Build instructions
# ==============================================================================

# Composite target for all examples (local to this module)
set(EXAMPLES_TARGETS "")

# Macro so that the EXAMPLES_TARGETS variable is updated correctly
# and can be used to create a composite target at the end of this file.
macro(add_example EXAMPLE_NAME SOURCE_FILE)
  message(STATUS "Adding example: ${EXAMPLE_NAME} from ${SOURCE_FILE}")
  set(EXAMPLE_TARGET_NAME "${META_MODULE_NAME}.${EXAMPLE_NAME}")
  add_executable(${EXAMPLE_TARGET_NAME} ${SOURCE_FILE})
  set_target_properties(
    ${EXAMPLE_TARGET_NAME}
    PROPERTIES
      FOLDER
        "Examples"
      OUTPUT_NAME
        ${EXAMPLE_TARGET_NAME}
  )
  target_compile_options(${EXAMPLE_TARGET_NAME} PRIVATE ${NOVA_COMMON_CXX_FLAGS})
  target_link_libraries(${EXAMPLE_TARGET_NAME} PRIVATE nova::base)
  arrange_target_files_for_ide(${EXAMPLE_TARGET_NAME})

  # Add to composite target list
  set(EXAMPLES_TARGETS ${EXAMPLES_TARGETS} ${EXAMPLE_TARGET_NAME} PARENT_SCOPE)

  NOVA_module_install(
    EXPORT ${META_MODULE_NAMESPACE}
    TARGETS
      ${EXAMPLE_TARGET_NAME}
  )
endmacro()

add_subdirectory("StateMachine_Door_1")
add_subdirectory("StateMachine_Door_2")
add_subdirectory("StateMachine_Door_3")

add_custom_target(Nova.Base.Examples ALL DEPENDS ${EXAMPLES_TARGETS})
