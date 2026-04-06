# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

# ------------------------------------------------------------------------------
# Function: nova_version_read
# Purpose:
#   Reads version information from a specified file and sets the variables
#   META_VERSION_MAJOR, META_VERSION_MINOR, and META_VERSION_PATCH.
#
# Arguments:
#   VERSION_FILE (optional): The path to the version file. Can be absolute
#   or relative to CMAKE_CURRENT_SOURCE_DIR. Defaults to "VERSION".
#
# Behavior:
#   This function reads a specified version file, ensuring it follows the format
#   "major.minor.patch." If the file path isn't provided, it defaults to "VERSION"
#   in the current source directory. The major, minor, and patch numbers are
#   extracted from the file content (which must be eactly `major.minor.patch`).
#   If the file isn't found or the format is incorrect, it outputs a fatal error.
#
# Variables Set:
#   META_VERSION_MAJOR: The major version number.
#   META_VERSION_MINOR: The minor version number.
#   META_VERSION_PATCH: The patch version number.
#
# Example Usage:
#   nova_version_read()  # Uses default "VERSION"
#   message(STATUS "Version: ${META_VERSION_MAJOR}.${META_VERSION_MINOR}.${META_VERSION_PATCH}")
# ------------------------------------------------------------------------------
function(nova_version_read)
  set(options)
  set(oneValueArgs VERSION_FILE)
  set(multiValueArgs)

  cmake_parse_arguments(
    x
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )

  # Use default "VERSION" if no version_file argument is provided
  set(
    version_file
    ${X_VERSION_FILE}
    "VERSION"
  )
  if(NOT IS_ABSOLUTE "${version_file}")
    cmake_path(
      SET
      version_file
      NORMALIZE
      "${CMAKE_CURRENT_SOURCE_DIR}/${version_file}"
    )
  endif()

  # Ensure the version file path is absolute or resolve it relative to the current source directory

  # Check if the version file exists
  if(NOT EXISTS "${version_file}")
    message(FATAL_ERROR "Version file not found: ${version_file}")
  endif()

  # Read the version file content
  file(READ "${version_file}" version)
  string(STRIP "${version}" version)

  # Use regex to match the version pattern and extract major, minor, patch
  string(
    REGEX
    MATCH
    "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$"
    VERSION_MATCHES
    "${version}"
  )

  # Ensure the version pattern is correctly matched
  if(NOT VERSION_MATCHES)
    message(
      FATAL_ERROR
      "Invalid version format (${version}) in file: ${version_file}. Expected format: major.minor.patch"
    )
  endif()

  # Extract and set version components
  set(META_VERSION_MAJOR "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(META_VERSION_MINOR "${CMAKE_MATCH_2}" PARENT_SCOPE)
  set(META_VERSION_PATCH "${CMAKE_MATCH_3}" PARENT_SCOPE)
endfunction()
