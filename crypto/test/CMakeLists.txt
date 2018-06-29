
list(APPEND CRYPTO_TEST_SRC
  hash_test.cpp
  keypair_test.cpp
  )

set(CRYPTO_TEST_LIBRARIES asap::crypto gtest_main gtest gmock)

asap_test(
  TARGET
  crypto_test
  SOURCES
  ${CRYPTO_TEST_SRC}
  PUBLIC_LIBRARIES
  ${CRYPTO_TEST_LIBRARIES}
)
set_tidy_target_properties(crypto_test_bin)
