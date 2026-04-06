##-----------------------------------------------------------------------------
# DetectCxxStandards.cmake
# Helper module to detect C++20 / C++23 support.
#
# API:
#   detect_cxx_standards(<PREFIX>)
#
# This function is silent (prints nothing). It sets the following variables in
# the caller scope using the provided PREFIX (no underscore added):
#   <PREFIX>_CXX20_SUPPORTED   -> TRUE/FALSE
#   <PREFIX>_CXX20_FLAG        -> string of working compiler flag (or empty)
#   <PREFIX>_CXX23_SUPPORTED   -> TRUE/FALSE
#   <PREFIX>_CXX23_FLAG        -> string of working compiler flag (or empty)
#
# Implementation notes:
#
# - The function uses try-compiles (CheckCXXSourceCompiles) with
#   CMAKE_REQUIRED_FLAGS to probe common -std=/std: variants for GCC/Clang
#   and MSVC. It verifies only that the compiler accepts the flag and can
#   compile a trivial program; it does not guarantee full language or
#   standard-library feature availability.
##-----------------------------------------------------------------------------

# Guard against double-include
if(DEFINED _DETECT_CXX_STANDARDS_INCLUDED)
  return()
endif()
set(_DETECT_CXX_STANDARDS_INCLUDED TRUE)

include(CheckCXXSourceCompiles)

function(detect_cxx_standards prefix)
  # Prepare flag lists covering GCC/Clang and MSVC variants
  set(
    _cxx20_flags
    "-std=c++20"
    "/std:c++20"
    "/std:c++2a"
  )
  set(
    _cxx23_flags
    "-std=c++23"
    "/std:c++23"
    "/std:c++2b"
    "/std:c++23preview"
    "/std:c++latest"
  )

  # Build variable names
  set(_out20_supported_var "${prefix}_CXX20_SUPPORTED")
  set(_out20_flag_var "${prefix}_CXX20_FLAG")
  set(_out23_supported_var "${prefix}_CXX23_SUPPORTED")
  set(_out23_flag_var "${prefix}_CXX23_FLAG")

  # Initialize outputs to safe defaults in caller scope
  set(${_out20_supported_var} FALSE PARENT_SCOPE)
  set(${_out20_flag_var} "" PARENT_SCOPE)
  set(${_out23_supported_var} FALSE PARENT_SCOPE)
  set(${_out23_flag_var} "" PARENT_SCOPE)

  # Save and restore CMAKE_REQUIRED_FLAGS
  set(_old_required_flags "${CMAKE_REQUIRED_FLAGS}")

  # Try helper
  foreach(_f IN LISTS _cxx20_flags)
    set(CMAKE_REQUIRED_FLAGS "${_f}")
    check_cxx_source_compiles(
      "int main() { return 0; }"
      _ok
    )
    if(_ok)
      set(${_out20_supported_var} TRUE PARENT_SCOPE)
      set(${_out20_flag_var} "${_f}" PARENT_SCOPE)
      break()
    endif()
  endforeach()

  foreach(_f IN LISTS _cxx23_flags)
    set(CMAKE_REQUIRED_FLAGS "${_f}")
    check_cxx_source_compiles(
      "int main() { return 0; }"
      _ok
    )
    if(_ok)
      set(${_out23_supported_var} TRUE PARENT_SCOPE)
      set(${_out23_flag_var} "${_f}" PARENT_SCOPE)
      break()
    endif()
  endforeach()

  # restore
  set(CMAKE_REQUIRED_FLAGS "${_old_required_flags}")
endfunction()
