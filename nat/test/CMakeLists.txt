
list(APPEND PORTFWD_TEST_SRC
  portfwd_test.cpp
  )

set(PORTFWD_TEST_LIBRARIES asap::nat gtest_main gtest gmock)

asap_test(
  TARGET
  portfwd_test
  SOURCES
  ${PORTFWD_TEST_SRC}
  PUBLIC_LIBRARIES
  ${PORTFWD_TEST_LIBRARIES}
)
set_tidy_target_properties(portfwd_test_bin)
