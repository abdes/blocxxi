# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

add_executable(mainline-node main.cpp)
set_target_properties(
  mainline-node
  PROPERTIES
    FOLDER
      "Examples"
)
target_compile_features(mainline-node PRIVATE cxx_std_20)
target_compile_options(mainline-node PRIVATE ${NOVA_COMMON_CXX_FLAGS})
find_package(Threads REQUIRED)
target_link_libraries(
  mainline-node
  PRIVATE
    nova::base
    blocxxi::p2p
    Threads::Threads
)
