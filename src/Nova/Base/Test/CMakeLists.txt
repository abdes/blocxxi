# ===-----------------------------------------------------------------------===#
# Distributed under the 3-Clause BSD License. See accompanying file LICENSE or
# copy at https://opensource.org/licenses/BSD-3-Clause.
# SPDX-License-Identifier: BSD-3-Clause
# ===-----------------------------------------------------------------------===#

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------

# ==============================================================================
# Build instructions
# ==============================================================================

# -- Library Link Test ---------------------------------------------------------

add_executable("Nova.Base.LinkTest" "Link_test.cpp")
set_target_properties(
  "Nova.Base.LinkTest"
  PROPERTIES
    FOLDER
      "Testing"
)
target_compile_options(
  "Nova.Base.LinkTest"
  PRIVATE
    ${NOVA_COMMON_CXX_FLAGS}
)
target_link_libraries("Nova.Base.LinkTest" PRIVATE nova::base)
add_test(NAME "Nova.Base Link Test" COMMAND "Nova.Base.LinkTest")

# -- GTest Test Programs -------------------------------------------------------

include(GTestHelpers)

m_gtest_program(Macros SOURCES "Macros_test.cpp")
m_gtest_program(Config SOURCES "Platform_test.cpp")
m_gtest_program(Types SOURCES "Types_test.cpp")
m_gtest_program(StringUtils SOURCES "StringUtils_test.cpp")
m_gtest_program(Finally SOURCES "Finally_test.cpp")
m_gtest_program(ScopeGuard SOURCES "ScopeGuard_test.cpp")

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  m_gtest_program(
    WinUtils
    SOURCES
      "Exceptions_test.cpp"
      "ComError_test.cpp"
  )
endif()

m_gtest_program(TypeList SOURCES "TypeList_test.cpp")

m_gtest_program(
  Resource
  SOURCES
    "Resource_test.cpp"
    "ResourceHandle_test.cpp"
    "ResourceTable_test.cpp"
)

m_gtest_program(StaticVector SOURCES "StaticVector_test.cpp")
m_gtest_program(
  Logging
  SOURCES
    "Logging_test.cpp"
    "Logging_vmodule_test.cpp"
)
m_gtest_program(
  StateMachine
  SOURCES
    "StateMachine_test.cpp"
    "StateMachine_doc_test.cpp"
)
m_gtest_program(NamedType SOURCES "NamedType_test.cpp")
m_gtest_program(ObserverPtr SOURCES "ObserverPtr_test.cpp")
m_gtest_program(
  EnumIndexArray
  SOURCES
    "EnumIndexArray_test.cpp"
    "EnumAsIndex_test.cpp"
)

# Hash utilities tests
m_gtest_program(Hash SOURCES "Hash_test.cpp")
m_gtest_program(Sha256 SOURCES "Sha256_test.cpp")

m_gtest_program(NoInline SOURCES "NoInline_test.cpp")
m_gtest_program(Result SOURCES "Result_test.cpp")
m_gtest_program(ReturnAddress SOURCES "ReturnAddress_test.cpp")
m_gtest_program(Unreachable SOURCES "Unreachable_test.cpp")
m_gtest_program(Uuid SOURCES "Uuid_test.cpp")
