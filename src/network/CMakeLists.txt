# Define the network library
add_library(dht_hunter_network
    udp_socket.cpp
    udp_server.cpp
    udp_client.cpp
    network_address.cpp
)

# Set include directories
target_include_directories(dht_hunter_network
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_network
    PUBLIC
    cxx_std_20
)

# Link with the logging and event libraries
target_link_libraries(dht_hunter_network
    PUBLIC
    dht_hunter_logforge
    dht_hunter_event
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(dht_hunter_network
        PUBLIC
        ws2_32
    )
endif()
