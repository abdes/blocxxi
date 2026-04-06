# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

add_subdirectory("Codec")
add_subdirectory("Crypto")
add_subdirectory("Nat")
add_subdirectory("Core")
add_subdirectory("Chain")
add_subdirectory("Storage")
add_subdirectory("Node")
add_subdirectory("Bitcoin")
add_subdirectory("P2P")

if(NOVA_BUILD_EXAMPLES)
  add_subdirectory("Examples")
endif()
