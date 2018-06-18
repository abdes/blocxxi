
list(APPEND NAT_PUBLIC_HEADERS
        "include/nat/error.h"
        "include/nat/port_mapper.h"
        "include/nat/upnp_port_mapper.h"
        "include/nat/nat.h"
        )

list(APPEND NAT_SRC
        "src/error.cpp"
        "src/nat.cpp"
        "src/upnp_port_mapper.cpp"
        ${NAT_PUBLIC_HEADERS}
        )

list(APPEND NAT_PUBLIC_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        )

set(NAT_PUBLIC_LIBRARIES
        libminiupnpc-static
        bloxi::common
        cryptopp-static
        Boost::boost
        Boost::system
        )

bloxi_library(
        TARGET
        bloxi_nat
        SOURCES
        ${NAT_SRC}
        EXPORT_NAME
        nat
        PUBLIC_INCLUDE_DIRS
        ${NAT_PUBLIC_INCLUDE_DIRS}
        PUBLIC_LIBRARIES
        ${NAT_PUBLIC_LIBRARIES}
)
set_tidy_target_properties(bloxi_nat)

add_subdirectory(test)

configure_doxyfile(NATLib
        "\"NAT Module\""
        "\"A module to provide NAT tools such as port forwarding, upnp, ...\""
        "${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include")
add_doxygen_target(NATLib)
