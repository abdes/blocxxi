
list(APPEND CRYPTO_PUBLIC_HEADERS
        "include/crypto/random.h"
        "include/crypto/hash.h"
        "include/crypto/keypair.h"
        )

list(APPEND CRYPTO_SRC
        "src/random.cpp"
        "src/hash.cpp"
        "src/keypair.cpp"
        ${CRYPTO_PUBLIC_HEADERS}
        )

list(APPEND CRYPTO_PUBLIC_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        )

set(CRYPTO_PUBLIC_LIBRARIES bloxi::codec bloxi::common cryptopp-static Boost::boost)

bloxi_library(
        TARGET
        bloxi_crypto
        SOURCES
        ${CRYPTO_SRC}
        EXPORT_NAME
        crypto
        PUBLIC_INCLUDE_DIRS
        ${CRYPTO_PUBLIC_INCLUDE_DIRS}
        PUBLIC_LIBRARIES
        ${CRYPTO_PUBLIC_LIBRARIES}
)

add_subdirectory(test)

configure_doxyfile(CryptoLib 
                   "\"Crypto Module\"" 
                   "\"A module to provide crypto functionality and the basic types using across the project for implementing hashing, digests, encryption, etc..\"" 
                   "${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include")
add_doxygen_target(CryptoLib)
