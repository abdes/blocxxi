# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

add_executable(blocxxi-hello-plugin main.cpp)
set_target_properties(
  blocxxi-hello-plugin
  PROPERTIES
    FOLDER
      "Examples"
)
target_compile_features(blocxxi-hello-plugin PRIVATE cxx_std_20)
target_compile_options(blocxxi-hello-plugin PRIVATE ${NOVA_COMMON_CXX_FLAGS})
target_link_libraries(
  blocxxi-hello-plugin
  PRIVATE
    nova::base
    blocxxi::node
)
