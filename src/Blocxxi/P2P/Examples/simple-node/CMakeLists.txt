# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

add_executable(
  simple-node
  main.cpp
  ../utils/console_runner.cpp
  ../utils/console_runner.h
  ../utils/runner_base.h
)
set_target_properties(simple-node PROPERTIES FOLDER "Examples")
target_compile_features(simple-node PRIVATE cxx_std_20)
target_compile_options(simple-node PRIVATE ${NOVA_COMMON_CXX_FLAGS})
find_package(Threads REQUIRED)
target_link_libraries(simple-node PRIVATE nova::base blocxxi::nat blocxxi::p2p Threads::Threads)
