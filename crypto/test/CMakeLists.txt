
list(APPEND CRYPTO_TEST_SRC
        hash_test.cpp
        keypair_test.cpp
        )

set(CRYPTO_TEST_LIBRARIES bloxi::crypto)

bloxi_test(
        TARGET
        crypto_test
        SOURCES
        ${CRYPTO_TEST_SRC}
        PUBLIC_LIBRARIES
        ${CRYPTO_TEST_LIBRARIES}
)