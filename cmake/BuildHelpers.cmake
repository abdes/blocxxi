# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

# ------------------------------------------------------------------------------
# Build Helpers to simplify target creation.
# ------------------------------------------------------------------------------

if(__build_helpers)
  return()
endif()
set(__build_helpers YES)

# We must run the following at "include" time, not at function call time, to
# find the path to this module rather than the path to a calling list file
get_filename_component(_build_helpers_dir ${CMAKE_CURRENT_LIST_FILE} PATH)

include(GenerateExportHeader)

# ------------------------------------------------------------------------------
# Meta information about this module.
#
# Of particular importance is the MODULE_NAME, which can be composed of multiple
# segments separated by '.' or '_'. In such case, these segments will be used
# as path segments for the `api_export` generated file, and as identifier segments
# in the corresponding `_API` compiler defines.
#
# For example, `Super.Hero.Module` will produce a file that can be included as
# "super/hero/module/api_export.h" and will provide the export macro as
# `SUPER_HERO_MODULE_API`.
#
# It is a common practice and a recommended one to use a target name for that
# module with the same name (i.e. Super.Hero.Module).
# ------------------------------------------------------------------------------

function(nova_module_declare)
  set(options WITHOUT_VERSION_H)
  set(
    oneValueArgs
    MODULE_NAME
    MODULE_TARGET_NAME
    DESCRIPTION
  )
  set(multiValueArgs)

  cmake_parse_arguments(
    x
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )

  if(NOT DEFINED x_MODULE_NAME)
    message(FATAL_ERROR "Module name is required.")
    return()
  endif()

  # Split the module_full_name into a list
  string(REPLACE "." ";" module_parts ${x_MODULE_NAME})

  # Extract the namespace
  list(GET module_parts 0 namespace)
  string(TOLOWER ${namespace} namespace)

  # Extract and process the module_name (remaining parts)
  list(REMOVE_AT module_parts 0)
  string(JOIN "." unqualified_module_name ${module_parts})
  string(TOLOWER ${unqualified_module_name} unqualified_module_name)
  string(REPLACE "." "-" unqualified_module_name ${unqualified_module_name})

  # Define the module's meta variables
  set(META_MODULE_NAME "${x_MODULE_NAME}" PARENT_SCOPE)
  set(META_MODULE_DESCRIPTION "${x_DESCRIPTION}" PARENT_SCOPE)
  set(META_MODULE_NAMESPACE "${namespace}" PARENT_SCOPE)
  set(META_MODULE_TARGET "${namespace}-${unqualified_module_name}" PARENT_SCOPE)
  set(
    META_MODULE_TARGET_ALIAS
    "${namespace}::${unqualified_module_name}"
    PARENT_SCOPE
  )

  # Generate the version.h file for the module
  if(NOT x_WITHOUT_VERSION_H)
    set(version_h_in "${_build_helpers_dir}/module_version.h.in")
    nova_module_name(
      SEGMENTS ${x_MODULE_NAME}
      OUTPUT_VARIABLE path_segments
      TO_LOWER
    )
    cmake_path(
      APPEND
      ${CMAKE_CURRENT_BINARY_DIR}
      "include"
      ${path_segments}
      "version.h"
      OUTPUT_VARIABLE version_h_file
    )
    configure_file("${version_h_in}" "${version_h_file}")
  endif()

  # Check if the module has been pushed on top of the hierarchy stack
  if(NOT ASAP_LOG_PROJECT_HIERARCHY MATCHES "(${META_MODULE_NAME})")
    message(
      AUTHOR_WARNING
      "Can't find module `${META_MODULE_NAME}` on the hierarchy stack. "
      "Please make sure it has been pushed with nova_push_module()."
    )
  endif()
endfunction()

function(nova_module_name)
  set(
    options
    TO_LOWER
    TO_UPPER
  )
  set(
    oneValueArgs
    SEGMENTS
    API_EXPORT_MACRO
    OUTPUT_VARIABLE
  )
  set(multiValueArgs)

  cmake_parse_arguments(
    x
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )

  if(NOT DEFINED x_OUTPUT_VARIABLE)
    message(
      FATAL_ERROR
      "Must specify an `OUTPUT_VARIABLE` to receive the result."
    )
    return()
  endif()

  if(x_SEGMENTS)
    set(module_name ${x_SEGMENTS})
  endif()
  if(x_API_EXPORT_MACRO)
    set(module_name ${x_API_EXPORT_MACRO})
  endif()
  if(NOT DEFINED module_name)
    message(
      FATAL_ERROR
      "Must specify `SEGMENTS` or `EXPORT_MACRO` and provide a module name."
    )
    return()
  endif()

  if(x_TO_LOWER AND x_TO_UPPER)
    message(
      FATAL_ERROR
      "Can only specify either `TO_LOWER` or `TO_UPPER` but not both."
    )
    return()
  endif()

  # Extract words from the identifier expecting it to be using '_' or '.' to
  # compose a hierarchy of segments
  string(REGEX MATCHALL "[A-Za-z][^_.]*" words ${module_name})

  # Process each word
  set(result)
  foreach(word IN LISTS words)
    if(x_TO_LOWER)
      string(TOLOWER "${word}" word)
    endif()
    if(x_TO_UPPER)
      string(TOUPPER "${word}" word)
    endif()
    list(APPEND result "${word}")
  endforeach()

  if(x_API_EXPORT_MACRO)
    string(JOIN "_" result ${result})
  endif()

  set(${x_OUTPUT_VARIABLE} "${result}" PARENT_SCOPE)
endfunction()

function(nova_generate_export_headers target module)
  nova_module_name(SEGMENTS ${module} OUTPUT_VARIABLE path_segments TO_LOWER)
  cmake_path(
    APPEND
    ${CMAKE_CURRENT_BINARY_DIR}
    "include"
    ${path_segments}
    "api_export.h"
    OUTPUT_VARIABLE export_file
  )

  nova_module_name(
    API_EXPORT_MACRO ${module}
    OUTPUT_VARIABLE export_base_name
    TO_UPPER
  )
  set(export_macro_name "${export_base_name}_API")

  generate_export_header(
    ${target}
    BASE_NAME ${export_base_name}
    EXPORT_FILE_NAME ${export_file}
    EXPORT_MACRO_NAME ${export_macro_name}
  )
endfunction()

function(arrange_target_files_for_ide target)
  set(options)
  set(oneValueArgs)
  set(multiValueArgs EXCLUDE_PATTERNS)
  cmake_parse_arguments(
    x
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )

  get_target_property(_all_sources ${target} SOURCES)

  # Normalize generator expressions in target sources so that files guarded by
  # platform conditions (e.g. $<$<BOOL:${WIN32}>:path/to/file.cpp>) are treated
  # as part of the target when checking for "missing" files.
  set(_all_files "")
  foreach(file IN LISTS _all_sources)
    list(APPEND _all_files "${file}")

    # Extract the guarded file path from common genex forms.
    if(file MATCHES "^\\$<.+:(.+)>$")
      list(APPEND _all_files "${CMAKE_MATCH_1}")
    endif()
  endforeach()

  # Group files for IDE
  foreach(file IN LISTS _all_files)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
      get_filename_component(file_path "${file}" PATH)
      if(file_path)
        string(REPLACE "/" "\\" group_path "${file_path}")
        source_group("src/${group_path}" FILES "${file}")
      else()
        source_group("src" FILES "${file}")
      endif()
    endif()
  endforeach()

  # Find all .h, .hpp, .c, .cpp files recursively
  file(
    GLOB_RECURSE _all_existing_files
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  )

  # Print files not in _all_files, excluding Test, Benchmarks, Examples
  set(_missing_files "")
  foreach(existing_file IN LISTS _all_existing_files)
    # Exclude if path contains Test, Benchmarks, Tools or Examples
    # (case-insensitive)
    string(TOLOWER "${existing_file}" _lower_file)
    set(_skip_file FALSE)
    if(
      _lower_file
        MATCHES
        "test/"
      OR
        _lower_file
          MATCHES
          "benchmarks/"
      OR
        _lower_file
          MATCHES
          "examples/"
      OR
        _lower_file
          MATCHES
          "tools/"
    )
      continue()
    endif()

    foreach(_pattern IN LISTS x_EXCLUDE_PATTERNS)
      string(TOLOWER "${_pattern}" _exclude_pattern)
      if(_exclude_pattern AND _lower_file MATCHES "${_exclude_pattern}")
        set(_skip_file TRUE)
        break()
      endif()
    endforeach()
    if(_skip_file)
      continue()
    endif()

    list(FIND _all_files "${existing_file}" _idx)
    if(_idx EQUAL -1)
      list(APPEND _missing_files "${existing_file}")
    endif()
  endforeach()

  if(_missing_files)
    message(
      AUTHOR_WARNING
      "The following files exist in the source directory but are NOT part of the target '${target}':"
    )
    foreach(missing_file IN LISTS _missing_files)
      message(NOTICE "  ${missing_file}")
    endforeach()
  endif()
endfunction()
