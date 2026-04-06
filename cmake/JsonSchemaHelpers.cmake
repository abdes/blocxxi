# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

# ------------------------------------------------------------------------------
# Helpers for embedding JSON schemas into generated C++ headers.
# ------------------------------------------------------------------------------
#
# Usage:
#
# include(JsonSchemaHelpers)
#
# nova_embed_json_schemas(
#   TARGET <cmake-target>
#   OUTPUT_HEADER <generated/header/path>
#   NAMESPACE <c++ namespace>
#   INCLUDE_BASE_DIR <include root to add to target include paths>
#   CHUNK_SIZE <max chars per raw-literal chunk>
#   SCHEMAS
#     <symbol_name_1> <schema_file_1>
#     <symbol_name_2> <schema_file_2>
# )
#
# Notes:
# - `SCHEMAS` is an alternating list of <symbol_name> <schema_file>.
# - `symbol_name` must be a valid C++ identifier.
# - relative schema file paths are resolved against CMAKE_CURRENT_SOURCE_DIR.
# - relative OUTPUT_HEADER / INCLUDE_BASE_DIR paths are resolved against
#   CMAKE_CURRENT_BINARY_DIR.
#
# The generated header uses chunked raw string literals to avoid compiler limits
# on a single string-literal/token size.

if(__json_schema_helpers)
  return()
endif()
set(__json_schema_helpers YES)

# We resolve this at include time so downstream calls remain independent from
# the call-site list file path.
get_filename_component(_json_schema_helpers_dir "${CMAKE_CURRENT_LIST_FILE}" PATH)

function(nova_embed_json_schemas)
  set(options)
  set(
    oneValueArgs
    TARGET
    OUTPUT_HEADER
    NAMESPACE
    INCLUDE_BASE_DIR
    CHUNK_SIZE
    OUT_HEADER_VARIABLE
  )
  set(multiValueArgs SCHEMAS)

  cmake_parse_arguments(
    x
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )

  if(NOT DEFINED x_TARGET OR x_TARGET STREQUAL "")
    message(FATAL_ERROR "nova_embed_json_schemas: TARGET is required.")
  endif()
  if(NOT TARGET "${x_TARGET}")
    message(
      FATAL_ERROR
      "nova_embed_json_schemas: target `${x_TARGET}` does not exist."
    )
  endif()
  if(NOT DEFINED x_OUTPUT_HEADER OR x_OUTPUT_HEADER STREQUAL "")
    message(FATAL_ERROR "nova_embed_json_schemas: OUTPUT_HEADER is required.")
  endif()
  if(NOT DEFINED x_NAMESPACE OR x_NAMESPACE STREQUAL "")
    message(FATAL_ERROR "nova_embed_json_schemas: NAMESPACE is required.")
  endif()
  if(NOT DEFINED x_SCHEMAS OR x_SCHEMAS STREQUAL "")
    message(FATAL_ERROR "nova_embed_json_schemas: SCHEMAS is required.")
  endif()

  if(NOT DEFINED x_CHUNK_SIZE OR x_CHUNK_SIZE STREQUAL "")
    set(x_CHUNK_SIZE 8192)
  endif()
  if(NOT x_CHUNK_SIZE MATCHES "^[1-9][0-9]*$")
    message(
      FATAL_ERROR
      "nova_embed_json_schemas: CHUNK_SIZE must be a positive integer."
    )
  endif()

  if(IS_ABSOLUTE "${x_OUTPUT_HEADER}")
    set(_output_header "${x_OUTPUT_HEADER}")
  else()
    cmake_path(
      ABSOLUTE_PATH
      x_OUTPUT_HEADER
      BASE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
      NORMALIZE
      OUTPUT_VARIABLE _output_header
    )
  endif()

  list(LENGTH x_SCHEMAS _schema_arg_count)
  math(EXPR _schema_arg_mod "${_schema_arg_count} % 2")
  if(NOT _schema_arg_mod EQUAL 0)
    message(
      FATAL_ERROR
      "nova_embed_json_schemas: SCHEMAS must be pairs of "
      "<symbol_name> <schema_file>."
    )
  endif()

  set(_schema_names "")
  set(_schema_files "")
  set(_schema_index 0)
  while(_schema_index LESS _schema_arg_count)
    list(GET x_SCHEMAS ${_schema_index} _schema_symbol)
    math(EXPR _schema_path_index "${_schema_index} + 1")
    list(GET x_SCHEMAS ${_schema_path_index} _schema_file_raw)

    if(NOT _schema_symbol MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
      message(
        FATAL_ERROR
        "nova_embed_json_schemas: invalid schema symbol `${_schema_symbol}`. "
        "Expected a valid C++ identifier."
      )
    endif()

    if(IS_ABSOLUTE "${_schema_file_raw}")
      set(_schema_file "${_schema_file_raw}")
    else()
      cmake_path(
        ABSOLUTE_PATH
        _schema_file_raw
        BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        NORMALIZE
        OUTPUT_VARIABLE _schema_file
      )
    endif()

    if(NOT EXISTS "${_schema_file}")
      message(
        FATAL_ERROR
        "nova_embed_json_schemas: schema file not found: `${_schema_file}`."
      )
    endif()

    list(APPEND _schema_names "${_schema_symbol}")
    list(APPEND _schema_files "${_schema_file}")
    math(EXPR _schema_index "${_schema_index} + 2")
  endwhile()

  get_filename_component(_output_header_dir "${_output_header}" DIRECTORY)
  file(MAKE_DIRECTORY "${_output_header_dir}")

  string(
    MD5
    _generation_id
    "${_output_header}|${x_NAMESPACE}|${x_CHUNK_SIZE}|${_schema_names}|${_schema_files}"
  )
  string(SUBSTRING "${_generation_id}" 0 16 _generation_id_short)

  set(
    _manifest_file
    "${CMAKE_CURRENT_BINARY_DIR}/nova_json_schemas_${_generation_id_short}.cmake"
  )

  file(WRITE "${_manifest_file}" "# Auto-generated by nova_embed_json_schemas\n")
  file(
    APPEND
    "${_manifest_file}"
    "set(NOVA_JSON_SCHEMA_OUTPUT_HEADER [==[${_output_header}]==])\n"
  )
  file(
    APPEND
    "${_manifest_file}"
    "set(NOVA_JSON_SCHEMA_NAMESPACE [==[${x_NAMESPACE}]==])\n"
  )
  file(
    APPEND
    "${_manifest_file}"
    "set(NOVA_JSON_SCHEMA_CHUNK_SIZE [==[${x_CHUNK_SIZE}]==])\n"
  )
  file(APPEND "${_manifest_file}" "set(NOVA_JSON_SCHEMA_NAMES)\n")
  file(APPEND "${_manifest_file}" "set(NOVA_JSON_SCHEMA_FILES)\n")

  list(LENGTH _schema_names _schema_count)
  math(EXPR _schema_last_index "${_schema_count} - 1")
  foreach(_idx RANGE 0 ${_schema_last_index})
    list(GET _schema_names ${_idx} _schema_symbol)
    list(GET _schema_files ${_idx} _schema_file)
    file(
      APPEND
      "${_manifest_file}"
      "list(APPEND NOVA_JSON_SCHEMA_NAMES [==[${_schema_symbol}]==])\n"
    )
    file(
      APPEND
      "${_manifest_file}"
      "list(APPEND NOVA_JSON_SCHEMA_FILES [==[${_schema_file}]==])\n"
    )
  endforeach()

  set(
    _generator_script
    "${_json_schema_helpers_dir}/GenerateEmbeddedJsonSchemas.cmake"
  )
  set(
    _stamp_file
    "${CMAKE_CURRENT_BINARY_DIR}/nova_json_schemas_${_generation_id_short}.stamp"
  )

  # Generate once at configure-time so compile steps always have the header
  # available even on generators that may schedule custom targets late.
  execute_process(
    COMMAND
      ${CMAKE_COMMAND}
      -DINPUT_MANIFEST:FILEPATH=${_manifest_file}
      -P "${_generator_script}"
    COMMAND_ERROR_IS_FATAL ANY
  )

  add_custom_command(
    OUTPUT "${_stamp_file}"
    BYPRODUCTS "${_output_header}"
    COMMAND
      ${CMAKE_COMMAND}
      -DINPUT_MANIFEST:FILEPATH=${_manifest_file}
      -P "${_generator_script}"
    COMMAND ${CMAKE_COMMAND} -E touch "${_stamp_file}"
    DEPENDS
      "${_manifest_file}"
      "${_generator_script}"
      ${_schema_files}
    VERBATIM
  )

  set(_generation_target "${x_TARGET}-json-schemas-${_generation_id_short}")
  add_custom_target(${_generation_target} DEPENDS "${_stamp_file}")
  add_dependencies("${x_TARGET}" "${_generation_target}")

  set_source_files_properties("${_output_header}" PROPERTIES GENERATED TRUE)
  target_sources("${x_TARGET}" PRIVATE "${_output_header}")

  if(DEFINED x_INCLUDE_BASE_DIR AND NOT x_INCLUDE_BASE_DIR STREQUAL "")
    if(IS_ABSOLUTE "${x_INCLUDE_BASE_DIR}")
      set(_include_base_dir "${x_INCLUDE_BASE_DIR}")
    else()
      cmake_path(
        ABSOLUTE_PATH
        x_INCLUDE_BASE_DIR
        BASE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        NORMALIZE
        OUTPUT_VARIABLE _include_base_dir
      )
    endif()
    target_include_directories(
      "${x_TARGET}"
      BEFORE
      PRIVATE
        "${_include_base_dir}"
    )
  endif()

  if(DEFINED x_OUT_HEADER_VARIABLE AND NOT x_OUT_HEADER_VARIABLE STREQUAL "")
    set(${x_OUT_HEADER_VARIABLE} "${_output_header}" PARENT_SCOPE)
  endif()
endfunction()
