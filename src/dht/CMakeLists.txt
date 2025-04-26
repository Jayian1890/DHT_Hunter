# Define the DHT library
add_library(dht_hunter_dht
    # Core components
    core/dht_node.cpp
    core/dht_config.cpp
    core/dht_constants.cpp
    core/dht_types.cpp
    core/routing_table.cpp

    # Network components
    network/socket_manager.cpp
    network/message_sender.cpp
    network/message_handler.cpp
    network/message.cpp
    network/query_message.cpp
    network/response_message.cpp
    network/error_message.cpp

    # Routing components
    routing/node_lookup.cpp
    routing/peer_lookup.cpp
    routing/routing_manager.cpp

    # Storage components
    storage/token_manager.cpp
    storage/peer_storage.cpp

    # Transaction components
    transactions/transaction_manager.cpp

    # Bootstrap components
    bootstrap/bootstrapper.cpp
)

# Set include directories
target_include_directories(dht_hunter_dht
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_dht
    PUBLIC
    cxx_std_20
)

# Link with the required libraries
target_link_libraries(dht_hunter_dht
    PUBLIC
    dht_hunter_logforge
    dht_hunter_network
    dht_hunter_event
    dht_hunter_bencode
    dht_hunter_util
)

# Add the test subdirectory
add_subdirectory(test)
