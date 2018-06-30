
list(APPEND CODEC_TEST_SRC
  base16_test.cpp
  )

set(CODEC_TEST_LIBRARIES asap::codec gtest_main gtest gmock)

asap_test(
  TARGET
  codec_test
  SOURCES
  ${CODEC_TEST_SRC}
  PUBLIC_LIBRARIES
  ${CODEC_TEST_LIBRARIES}
)