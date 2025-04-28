# Add all components in the correct dependency order
add_subdirectory(types)     # Types component
add_subdirectory(util)      # Utility functions
add_subdirectory(network)   # Network component
add_subdirectory(bencode)   # Bencode component
add_subdirectory(dht)       # DHT component
add_subdirectory(bittorrent) # BitTorrent component
add_subdirectory(unified_event) # Unified event system

# Removed components:
# - crawler (not used in the build)
# - metadata (not used in the build)
# - service (not used in the build)
# - storage (not used in the build)
# - ui (not used in the build)

# Main executable
add_executable(dht_hunter main.cpp)

# Link with component libraries
target_link_libraries(dht_hunter
        PRIVATE
        dht_hunter_types
        dht_hunter_util
        dht_hunter_network
        dht_hunter_dht
        dht_hunter_bittorrent
        dht_hunter_unified_event
)

# Set include directories
target_include_directories(dht_hunter
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# Install the executable
install(TARGETS dht_hunter
        RUNTIME DESTINATION bin
)
