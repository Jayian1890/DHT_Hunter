# Add subdirectories for each component
add_subdirectory(logforge)
add_subdirectory(network)
add_subdirectory(dht)
add_subdirectory(bencode)
add_subdirectory(metadata)
add_subdirectory(crawler)
add_subdirectory(ui)
add_subdirectory(storage)

# Main executable
add_executable(dht_hunter main.cpp)

# Link with all libraries
target_link_libraries(dht_hunter
    PRIVATE
    dht_hunter_logforge
    dht_hunter_network
    dht_hunter_dht
    dht_hunter_bencode
    dht_hunter_metadata
    dht_hunter_crawler
    dht_hunter_ui
    dht_hunter_storage
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
