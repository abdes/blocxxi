# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

m_gtest_program(
  Unit
  SOURCES
    main.cpp
    krpc_test.cpp
    mainline_node_test.cpp
    mainline_session_test.cpp
    node_test.cpp
    channel_test.cpp
)
