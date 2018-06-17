
list(APPEND PORTFWD_TEST_SRC
        portfwd_test.cpp
        )

set(PORTFWD_TEST_LIBRARIES bloxi::nat)

bloxi_test(
        TARGET
        portfwd_test
        SOURCES
        ${PORTFWD_TEST_SRC}
        PUBLIC_LIBRARIES
        ${PORTFWD_TEST_LIBRARIES}
)