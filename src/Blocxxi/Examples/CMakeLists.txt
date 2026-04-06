# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

add_subdirectory("hello-plugin")
add_subdirectory("local-custom-chain")
add_subdirectory("bitcoin-observer")
add_subdirectory("bitcoin-mempool-analyzer")
add_subdirectory("bitcoin-event-reader")

if(NOVA_BUILD_TESTS)
  add_test(NAME Blocxxi.Examples.HelloPlugin COMMAND blocxxi-hello-plugin)
  add_test(
    NAME Blocxxi.Examples.BitcoinObserver
    COMMAND
      blocxxi-bitcoin-observer
  )
  add_test(
    NAME Blocxxi.Examples.LocalCustomChain
    COMMAND
      "${CMAKE_COMMAND}" -DEXE=$<TARGET_FILE:blocxxi-local-custom-chain> -P
      "${CMAKE_SOURCE_DIR}/cmake/RunBlocxxiLocalCustomChainSmoke.cmake"
  )
  add_test(
    NAME Blocxxi.Examples.BitcoinMempoolAnalyzer
    COMMAND
      blocxxi-bitcoin-mempool-analyzer
  )
  add_test(
    NAME Blocxxi.Examples.BitcoinEventReader
    COMMAND
      blocxxi-bitcoin-event-reader
  )
endif()
