
# CryptoPP
# Original at https://www.cryptopp.com/cryptopp700.zip
# Download googletest
download_project(
        PROJ cryptopp
        URL ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cryptopp/cryptopp820.zip
        URL_HASH SHA256=1e5052e356bdefdfc742d0cfd480381a15d08d3a78c8a0383139b9fbe84d0dd5
)

set(CRYPTOPP_CMAKE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cryptopp-cmake)
set(CRYPTOPP_SRC_DIR ${CMAKE_BINARY_DIR}/cryptopp-src)

# Copy files from cryptopp-cmake to the working sources dir
configure_file(
        ${CRYPTOPP_CMAKE_DIR}/CMakeLists.txt
        ${CRYPTOPP_SRC_DIR}/CMakeLists.txt COPYONLY)
configure_file(
        ${CRYPTOPP_CMAKE_DIR}/cryptopp-config.cmake
        ${CRYPTOPP_SRC_DIR}/cryptopp-config.cmake COPYONLY)

# Let cmake do the rest using the cryptopp-cmake CMakeLists.txt
add_subdirectory(${CMAKE_BINARY_DIR}/cryptopp-src ${CMAKE_BINARY_DIR}/cryptopp-build)

# Add the include directory
include_directories(${CMAKE_BINARY_DIR}/cryptopp-src)

