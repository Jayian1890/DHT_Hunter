# Add only the necessary components
add_subdirectory(logforge)  # Logging component
add_subdirectory(network)   # Network component
add_subdirectory(event)     # Event system
add_subdirectory(util)      # Utility functions
add_subdirectory(bencode)   # Bencode component
add_subdirectory(dht)       # DHT component

# Main executable
add_executable(dht_hunter main.cpp)

# Link with component libraries
target_link_libraries(dht_hunter
    PRIVATE
    dht_hunter_logforge
    dht_hunter_network
    dht_hunter_event
    dht_hunter_util
    dht_hunter_dht
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
