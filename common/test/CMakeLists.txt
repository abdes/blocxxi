
list(APPEND COMMON_TEST_SRC
  assert_test.cpp
  logging_test.cpp
)

set(COMMON_TEST_LIBRARIES bloxi::common)

bloxi_test(
  TARGET
    common_test
  SOURCES
    ${COMMON_TEST_SRC}
  PUBLIC_LIBRARIES
    ${COMMON_TEST_LIBRARIES}
)