# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

add_executable(blocxxi-bitcoin-event-reader main.cpp)
set_target_properties(
  blocxxi-bitcoin-event-reader
  PROPERTIES
    FOLDER
      "Examples"
)
target_compile_features(blocxxi-bitcoin-event-reader PRIVATE cxx_std_20)
target_compile_options(
  blocxxi-bitcoin-event-reader
  PRIVATE
    ${NOVA_COMMON_CXX_FLAGS}
)
target_link_libraries(
  blocxxi-bitcoin-event-reader
  PRIVATE
    nova::base
    blocxxi::bitcoin
    blocxxi::p2p
)
