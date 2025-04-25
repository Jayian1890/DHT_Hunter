# Define the DHT library
add_library(dht_hunter_dht
    types.cpp
    message.cpp
    query_message.cpp
    response_message.cpp
    error_message.cpp
    routing_table.cpp
    dht_node.cpp
    node_lookup.cpp
    get_peers_lookup.cpp
    peer_storage.cpp
    routing_table_persistence.cpp
    dht_node_persistence.cpp
    transaction_persistence.cpp
    dht_node_transaction_persistence.cpp
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
