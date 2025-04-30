# Define the DHT library
add_library(dht_hunter_dht
        # Types components have been moved to the types module

        # Core components
        core/dht_node.cpp
        core/dht_event_handlers.cpp
        core/dht_config.cpp
        core/dht_constants.cpp
        core/routing_table.cpp
        core/persistence_manager.cpp

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
        routing/components/base_routing_component.cpp
        routing/components/node_verifier_component.cpp
        routing/components/bucket_refresh_component.cpp

        # Node Lookup components
        node_lookup/node_lookup.cpp
        node_lookup/node_lookup_query_manager.cpp
        node_lookup/node_lookup_response_handler.cpp
        # node_lookup_utils.cpp is now empty as implementation moved to util/node_utils.cpp
        node_lookup/node_lookup_utils.cpp
        node_lookup/components/base_node_lookup_component.cpp
        node_lookup/components/node_lookup_state.cpp
        node_lookup/components/node_lookup_manager.cpp

        # Peer Lookup components
        peer_lookup/peer_lookup.cpp
        peer_lookup/peer_lookup_query_manager.cpp
        peer_lookup/peer_lookup_announce_manager.cpp
        peer_lookup/peer_lookup_response_handler.cpp
        # peer_lookup_utils.cpp is now empty as implementation moved to util/node_utils.cpp
        peer_lookup/peer_lookup_utils.cpp
        peer_lookup/components/base_peer_lookup_component.cpp
        peer_lookup/components/peer_lookup_state.cpp

        # Storage components
        storage/token_manager.cpp
        storage/peer_storage.cpp

        # Transaction components
        transactions/transaction_manager.cpp

        # Bootstrap components
        bootstrap/bootstrapper.cpp
        bootstrap/components/base_bootstrap_component.cpp
        bootstrap/components/bootstrap_manager.cpp

        # Crawler components
        crawler/crawler.cpp

        # Event components have been moved to the unified event system

        # Service components
        services/statistics_service.cpp

        # Extension components
        extensions/factory/extension_factory.cpp
        extensions/implementations/mainline_dht.cpp
        extensions/implementations/kademlia_dht.cpp
        extensions/implementations/azureus_dht.cpp
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
        dht_hunter_utility
        dht_hunter_network
        dht_hunter_unified_event
        dht_hunter_bencode
)

