# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

# Disable MSVC warning C4324 for all targets in subdirectories
if(MSVC)
  add_compile_options(/wd4324)
endif()

add_subdirectory("Base")
add_subdirectory("Testing")
