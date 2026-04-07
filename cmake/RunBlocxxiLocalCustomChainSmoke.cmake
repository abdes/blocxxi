# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

if(NOT DEFINED EXE)
  message(FATAL_ERROR "EXE is required")
endif()

set(smoke_root "${CMAKE_CURRENT_BINARY_DIR}/blocxxi-local-custom-chain-smoke")
file(REMOVE_RECURSE "${smoke_root}")
file(MAKE_DIRECTORY "${smoke_root}")

execute_process(
  COMMAND
    "${EXE}" --storage-root "${smoke_root}" --payload "mint:1"
  RESULT_VARIABLE first_status
  OUTPUT_VARIABLE first_output
  ERROR_VARIABLE first_error
)
if(NOT first_status EQUAL 0)
  message(
    FATAL_ERROR
    "first local-custom-chain smoke run failed\nstdout:\n${first_output}\nstderr:\n${first_error}"
  )
endif()

execute_process(
  COMMAND
    "${EXE}" --storage-root "${smoke_root}" --payload "mint:2"
  RESULT_VARIABLE second_status
  OUTPUT_VARIABLE second_output
  ERROR_VARIABLE second_error
)
if(NOT second_status EQUAL 0)
  message(
    FATAL_ERROR
    "second local-custom-chain smoke run failed\nstdout:\n${second_output}\nstderr:\n${second_error}"
  )
endif()

string(FIND "${first_output}" "height=1" first_height)
string(FIND "${first_output}" "payload=mint:1" first_payload)
string(FIND "${second_output}" "height=2" second_height)
string(FIND "${second_output}" "payload=mint:2" second_payload)

if(
  first_height
    EQUAL
    -1
  OR
    first_payload
      EQUAL
      -1
  OR
    second_height
      EQUAL
      -1
  OR
    second_payload
      EQUAL
      -1
)
  message(
    FATAL_ERROR
    "unexpected smoke output\nfirst:\n${first_output}\nsecond:\n${second_output}"
  )
endif()
