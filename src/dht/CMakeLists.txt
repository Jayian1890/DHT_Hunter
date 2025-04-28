# Define the DHT library
add_library(dht_hunter_dht
        # Types components have been moved to the types module

        # Core components
        core/dht_node.cpp
        core/dht_event_handlers.cpp
        core/dht_config.cpp
        core/dht_constants.cpp
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
        routing/routing_manager.cpp
        routing/node_verifier.cpp

        # Node Lookup components
        node_lookup/node_lookup.cpp
        node_lookup/node_lookup_query_manager.cpp
        node_lookup/node_lookup_response_handler.cpp
        node_lookup/node_lookup_utils.cpp

        # Peer Lookup components
        peer_lookup/peer_lookup.cpp
        peer_lookup/peer_lookup_query_manager.cpp
        peer_lookup/peer_lookup_announce_manager.cpp
        peer_lookup/peer_lookup_response_handler.cpp
        peer_lookup/peer_lookup_utils.cpp

        # Storage components
        storage/token_manager.cpp
        storage/peer_storage.cpp

        # Transaction components
        transactions/transaction_manager.cpp

        # Bootstrap components
        bootstrap/bootstrapper.cpp

        # Crawler components
        crawler/crawler.cpp

        # Event components have been moved to the unified event system

        # Service components
        services/statistics_service.cpp

        # Extension components
        extensions/mainline_dht.cpp
        extensions/kademlia_dht.cpp
        extensions/azureus_dht.cpp
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
        dht_hunter_types
        dht_hunter_network
        dht_hunter_unified_event
        dht_hunter_bencode
        dht_hunter_util
)

