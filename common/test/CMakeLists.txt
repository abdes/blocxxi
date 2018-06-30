
list(APPEND COMMON_TEST_SRC
  assert_test.cpp
  logging_test.cpp
  main.cpp
)

set(COMMON_TEST_LIBRARIES asap::common Catch2)

asap_test(
  TARGET
    common_test
  SOURCES
    ${COMMON_TEST_SRC}
  PUBLIC_LIBRARIES
    ${COMMON_TEST_LIBRARIES}
)
set_tidy_target_properties(common_test_bin)
