# Add subdirectories for each component in dependency order
add_subdirectory(logforge)  # Base logging component
add_subdirectory(util)      # Utility functions
add_subdirectory(network)   # Network functionality
add_subdirectory(bencode)   # Bencode parsing
add_subdirectory(dht)       # DHT protocol implementation
add_subdirectory(metadata)  # Metadata handling
add_subdirectory(storage)   # Storage functionality
add_subdirectory(crawler)   # DHT crawler
add_subdirectory(ui)        # User interface

# Main executable
add_executable(dht_hunter main.cpp)

# Link with all component libraries
target_link_libraries(dht_hunter
    PRIVATE
    dht_hunter_logforge
    dht_hunter_util
    dht_hunter_network
    dht_hunter_dht
    dht_hunter_bencode
    dht_hunter_metadata
    dht_hunter_storage
    dht_hunter_crawler
    dht_hunter_ui
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
