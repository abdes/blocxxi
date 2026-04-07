# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

add_executable(blocxxi-local-custom-chain main.cpp)
set_target_properties(
  blocxxi-local-custom-chain
  PROPERTIES
    FOLDER
      "Examples"
)
target_compile_features(blocxxi-local-custom-chain PRIVATE cxx_std_20)
target_compile_options(
  blocxxi-local-custom-chain
  PRIVATE
    ${NOVA_COMMON_CXX_FLAGS}
)
target_link_libraries(
  blocxxi-local-custom-chain
  PRIVATE
    nova::base
    blocxxi::node
)
