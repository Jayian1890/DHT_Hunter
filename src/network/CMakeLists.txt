# Define the network library
add_library(dht_hunter_network
    network_address.cpp
    socket.cpp
    socket_factory.cpp
    socket_impl_common.cpp
    socket_impl_core.cpp
    tcp_socket_impl.cpp
    udp_socket_impl.cpp
    platform/windows/socket_impl.cpp
    platform/unix/socket_impl.cpp
)

# Set include directories
target_include_directories(dht_hunter_network
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_network
    PUBLIC
    cxx_std_17
)

# Link with the logging library
target_link_libraries(dht_hunter_network
    PUBLIC
    dht_hunter_logforge
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(dht_hunter_network
        PUBLIC
        ws2_32
    )
endif()
