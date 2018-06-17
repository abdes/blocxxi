
# Boost
set(BOOST_URL https://dl.bintray.com/boostorg/release/1.67.0/source/boost_1_67_0.tar.bz2 CACHE STRING "Boost download URL" FORCE)
set(BOOST_URL_SHA256 2684c972994ee57fc5632e03bf044746f6eb45d4920c343937a465fd67a5adba CACHE STRING "Boost download URL SHA256 checksum" FORCE)
add_subdirectory(boost-cmake)

add_subdirectory(gsl-lite)
add_subdirectory(spdlog)
add_subdirectory(miniupnpc)
