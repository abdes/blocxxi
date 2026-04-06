# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

# ------------------------------------------------------------------------------
# Build-time generator backend for embedded JSON schema headers.
# ------------------------------------------------------------------------------
#
# This script is intentionally executed with:
#   cmake -DINPUT_MANIFEST=<path> -P GenerateEmbeddedJsonSchemas.cmake
#
# It is the implementation backend used by `nova_embed_json_schemas()` from
# `cmake/JsonSchemaHelpers.cmake`.
#
# Why this script exists (instead of putting all logic in the helper):
# - the helper runs at CMake configure time (declares build graph/API),
# - this script runs at build time (generates files from declared inputs),
# - this separation preserves proper incremental rebuild behavior and avoids
#   fragile/huge inline command strings in add_custom_command().
#
# Contract:
# - Required input variable: INPUT_MANIFEST
# - INPUT_MANIFEST must define:
#     NOVA_JSON_SCHEMA_OUTPUT_HEADER : absolute output header path
#     NOVA_JSON_SCHEMA_NAMESPACE     : target C++ namespace
#     NOVA_JSON_SCHEMA_CHUNK_SIZE    : max chars per raw-literal chunk
#     NOVA_JSON_SCHEMA_NAMES         : list of C++ symbol names
#     NOVA_JSON_SCHEMA_FILES         : list of schema file paths
# - Names/files lists must have equal length and matching indices.
#
# Output:
# - Writes NOVA_JSON_SCHEMA_OUTPUT_HEADER containing:
#     inline constexpr std::string_view <symbol> = ...;
#   for each schema pair in the manifest.
#
# Implementation details:
# - line endings are normalized to '\n' for deterministic output,
# - schemas are emitted as chunked raw string literals to avoid compiler limits
#   on very large single literals/tokens (notably on MSVC).
#
if(NOT DEFINED INPUT_MANIFEST OR INPUT_MANIFEST STREQUAL "")
  message(FATAL_ERROR "INPUT_MANIFEST is required.")
endif()
# Be defensive against callers that accidentally pass quoted values.
string(REGEX REPLACE "^\"(.*)\"$" "\\1" INPUT_MANIFEST "${INPUT_MANIFEST}")
string(REGEX REPLACE "^\\\\\"(.*)\\\\\"$" "\\1" INPUT_MANIFEST "${INPUT_MANIFEST}")
if(NOT EXISTS "${INPUT_MANIFEST}")
  message(FATAL_ERROR "Manifest file not found: ${INPUT_MANIFEST}")
endif()

include("${INPUT_MANIFEST}")

if(
  NOT
    DEFINED
      NOVA_JSON_SCHEMA_OUTPUT_HEADER
  OR
    NOVA_JSON_SCHEMA_OUTPUT_HEADER
      STREQUAL
      ""
)
  message(FATAL_ERROR "Manifest is missing NOVA_JSON_SCHEMA_OUTPUT_HEADER.")
endif()
if(
  NOT
    DEFINED
      NOVA_JSON_SCHEMA_NAMESPACE
  OR
    NOVA_JSON_SCHEMA_NAMESPACE
      STREQUAL
      ""
)
  message(FATAL_ERROR "Manifest is missing NOVA_JSON_SCHEMA_NAMESPACE.")
endif()
if(
  NOT
    DEFINED
      NOVA_JSON_SCHEMA_CHUNK_SIZE
  OR
    NOVA_JSON_SCHEMA_CHUNK_SIZE
      STREQUAL
      ""
)
  message(FATAL_ERROR "Manifest is missing NOVA_JSON_SCHEMA_CHUNK_SIZE.")
endif()

if(NOT NOVA_JSON_SCHEMA_CHUNK_SIZE MATCHES "^[1-9][0-9]*$")
  message(FATAL_ERROR "NOVA_JSON_SCHEMA_CHUNK_SIZE must be a positive integer.")
endif()

if(NOT DEFINED NOVA_JSON_SCHEMA_NAMES OR NOT DEFINED NOVA_JSON_SCHEMA_FILES)
  message(
    FATAL_ERROR
    "Manifest is missing NOVA_JSON_SCHEMA_NAMES/NOVA_JSON_SCHEMA_FILES."
  )
endif()

list(LENGTH NOVA_JSON_SCHEMA_NAMES _schema_name_count)
list(LENGTH NOVA_JSON_SCHEMA_FILES _schema_file_count)

if(_schema_name_count EQUAL 0)
  message(FATAL_ERROR "No schema entries were provided.")
endif()
if(NOT _schema_name_count EQUAL _schema_file_count)
  message(
    FATAL_ERROR
    "Schema name/file count mismatch: names=${_schema_name_count} files=${_schema_file_count}."
  )
endif()

function(_nova_append_schema_literal_chunks output_file symbol_name schema_text chunk_size)
  if(NOT symbol_name MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
    message(FATAL_ERROR "Invalid schema symbol name: ${symbol_name}")
  endif()

  string(LENGTH "${schema_text}" _schema_len)
  file(
    APPEND
    "${output_file}"
    "inline constexpr std::string_view ${symbol_name} =\n"
  )

  if(_schema_len EQUAL 0)
    file(APPEND "${output_file}" "\"\";\n\n")
    return()
  endif()

  set(_offset 0)
  while(_offset LESS _schema_len)
    math(EXPR _remaining "${_schema_len} - ${_offset}")
    if(_remaining GREATER chunk_size)
      set(_count ${chunk_size})
    else()
      set(_count ${_remaining})
    endif()

    string(SUBSTRING "${schema_text}" ${_offset} ${_count} _chunk)

    # Raw string delimiters are compiler-limited (MSVC: <= 16 chars), so keep
    # delimiter candidates short and deterministic.
    string(MD5 _chunk_hash "${symbol_name}|${_offset}|${_chunk}")
    set(_delimiter_seed "d${_chunk_hash}")

    set(_attempt 0)
    set(_max_attempts 1000)
    set(_delimiter "")
    while(TRUE)
      if(_attempt EQUAL 0)
        string(SUBSTRING "${_delimiter_seed}" 0 15 _candidate)
      else()
        set(_suffix "${_attempt}")
        string(LENGTH "${_suffix}" _suffix_len)
        math(EXPR _prefix_len "15 - ${_suffix_len}")
        if(_prefix_len LESS 1)
          message(FATAL_ERROR "Failed generating raw string delimiter prefix.")
        endif()
        string(SUBSTRING "${_delimiter_seed}" 0 ${_prefix_len} _prefix)
        set(_candidate "${_prefix}${_suffix}")
      endif()

      string(LENGTH "${_candidate}" _candidate_len)
      if(_candidate_len GREATER 16)
        message(
          FATAL_ERROR
          "Internal error: raw string delimiter exceeds 16 chars."
        )
      endif()

      string(FIND "${_chunk}" ")${_candidate}\"" _delimiter_collision)
      if(_delimiter_collision EQUAL -1)
        set(_delimiter "${_candidate}")
        break()
      endif()

      math(EXPR _attempt "${_attempt} + 1")
      if(_attempt GREATER _max_attempts)
        message(FATAL_ERROR "Failed to generate collision-free raw delimiter.")
      endif()
    endwhile()

    file(APPEND "${output_file}" "R\"${_delimiter}(${_chunk})${_delimiter}\"\n")
    math(EXPR _offset "${_offset} + ${_count}")
  endwhile()

  file(APPEND "${output_file}" ";\n\n")
endfunction()

get_filename_component(
  _output_dir "${NOVA_JSON_SCHEMA_OUTPUT_HEADER}" DIRECTORY
)
file(MAKE_DIRECTORY "${_output_dir}")

file(
  WRITE
  "${NOVA_JSON_SCHEMA_OUTPUT_HEADER}"
  "// GENERATED FILE - DO NOT EDIT.\n"
)
file(APPEND "${NOVA_JSON_SCHEMA_OUTPUT_HEADER}" "#pragma once\n\n")
file(
  APPEND
  "${NOVA_JSON_SCHEMA_OUTPUT_HEADER}"
  "#include <string_view>\n\n"
)
file(
  APPEND
  "${NOVA_JSON_SCHEMA_OUTPUT_HEADER}"
  "namespace ${NOVA_JSON_SCHEMA_NAMESPACE} {\n\n"
)

math(EXPR _schema_last_index "${_schema_name_count} - 1")
foreach(_index RANGE 0 ${_schema_last_index})
  list(GET NOVA_JSON_SCHEMA_NAMES ${_index} _schema_symbol)
  list(GET NOVA_JSON_SCHEMA_FILES ${_index} _schema_file)

  # Backward-compat migration: older generated manifests may still reference
  # the pre-rename import schema filename. Remap to the canonical name so stale
  # build trees continue to work until next reconfigure.
  if(NOT EXISTS "${_schema_file}")
    get_filename_component(_schema_file_name "${_schema_file}" NAME)
    if(_schema_file_name STREQUAL "import-manifest.schema.json")
      get_filename_component(_schema_file_dir "${_schema_file}" DIRECTORY)
      set(_schema_file_candidate
        "${_schema_file_dir}/oxygen.import-manifest.schema.json"
      )
      if(EXISTS "${_schema_file_candidate}")
        set(_schema_file "${_schema_file_candidate}")
      endif()
    endif()
  endif()

  if(NOT EXISTS "${_schema_file}")
    message(FATAL_ERROR "Schema file not found: ${_schema_file}")
  endif()

  file(READ "${_schema_file}" _schema_text)
  # Normalize line endings for deterministic output across platforms.
  string(REPLACE "\r\n" "\n" _schema_text "${_schema_text}")
  string(REPLACE "\r" "\n" _schema_text "${_schema_text}")

  file(
    APPEND
    "${NOVA_JSON_SCHEMA_OUTPUT_HEADER}"
    "// Embedded from: ${_schema_file}\n"
  )
  _nova_append_schema_literal_chunks(
    "${NOVA_JSON_SCHEMA_OUTPUT_HEADER}"
    "${_schema_symbol}"
    "${_schema_text}"
    "${NOVA_JSON_SCHEMA_CHUNK_SIZE}"
  )
endforeach()

file(
  APPEND
  "${NOVA_JSON_SCHEMA_OUTPUT_HEADER}"
  "} // namespace ${NOVA_JSON_SCHEMA_NAMESPACE}\n"
)
