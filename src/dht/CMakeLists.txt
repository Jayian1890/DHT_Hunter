# Define the DHT library
add_library(dht_hunter_dht
    # Core components
    core/dht_node.cpp
    core/dht_node_config.cpp
    core/dht_constants.cpp
    core/dht_types.cpp
    core/dht_routing_table.cpp

    # Network components
    network/dht_socket_manager.cpp
    network/dht_message_sender.cpp
    network/dht_message_handler.cpp
    network/dht_message.cpp
    network/dht_query_message.cpp
    network/dht_response_message.cpp
    network/dht_error_message.cpp

    # Routing components
    routing/dht_routing_manager.cpp
    routing/dht_node_lookup.cpp
    routing/dht_peer_lookup.cpp

    # Storage components
    storage/dht_token_manager.cpp
    storage/dht_peer_storage.cpp

    # Transaction components
    transactions/dht_transaction_manager.cpp
    transactions/dht_transaction_persistence.cpp

    # Persistence components
    persistence/dht_persistence_manager.cpp

    # Bootstrap components
    bootstrap/dht_bootstrapper.cpp

    # Legacy files have been removed
)

# Set include directories
target_include_directories(dht_hunter_dht
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_dht
    PUBLIC
    cxx_std_17
)

# Link with other libraries
target_link_libraries(dht_hunter_dht
    PUBLIC
    dht_hunter_logforge
    dht_hunter_bencode
    dht_hunter_network
)
