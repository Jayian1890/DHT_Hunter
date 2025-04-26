# Define the network library
add_library(dht_hunter_network
    # Address-related files
    address/network_address.cpp
    address/endpoint.cpp
    address/address_resolver.cpp
    address/ipv4_resolver.cpp

    # Socket-related files
    socket/socket.cpp
    socket/socket_error.cpp
    socket/udp_socket.cpp

    # Platform-specific files
    platform/unix/socket_impl.cpp

    # Async-related files
    async/io_multiplexer.cpp
    async/async_socket.cpp
    async/async_socket_factory.cpp

    # Buffer-related files
    buffer/buffer.cpp
    buffer/buffer_pool.cpp
    buffer/message_buffer.cpp

    # Rate-limiting files
    rate/rate_limiter.cpp
    rate/connection_throttler.cpp
    rate/burst_controller.cpp

    # Reliability-related files
    reliability/udp_reliability.cpp
    reliability/retry_manager.cpp

    # Connection-related files
    connection/connection_pool.cpp
    connection/connection_pool_manager.cpp

    # Batch-related files
    batch/udp_message_batcher.cpp

    # Platform-specific multiplexers
    platform/unix/select_multiplexer.cpp
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
