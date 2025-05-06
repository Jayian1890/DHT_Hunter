# Define the BitTorrent library
add_library(dht_hunter_bittorrent
        bt_message_handler.cpp
        metadata/metadata_exchange.cpp
        metadata/metadata_acquisition_manager.cpp
        metadata/peer_health_tracker.cpp
        metadata/dht_metadata_provider.cpp
        metadata/connection_pool.cpp
        tracker/http_tracker.cpp
        tracker/udp_tracker.cpp
        tracker/tracker_manager.cpp
)

# Find libcurl
find_package(CURL REQUIRED)

# Link with other libraries
target_link_libraries(dht_hunter_bittorrent
        PUBLIC
        dht_hunter_core
        dht_hunter_types
        dht_hunter_utility
        dht_hunter_network
        dht_hunter_dht
        dht_hunter_bencode
        dht_hunter_unified_event
        ${CURL_LIBRARIES}
)

# Set include directories
target_include_directories(dht_hunter_bittorrent
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CURL_INCLUDE_DIRS}
)

# Install the library
install(TARGETS dht_hunter_bittorrent
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)
