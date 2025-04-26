# Define the BitTorrent library
add_library(dht_hunter_bittorrent
    bt_message_handler.cpp
)

# Link with other libraries
target_link_libraries(dht_hunter_bittorrent
    PUBLIC
    dht_hunter_core
    dht_hunter_logforge
    dht_hunter_network
    dht_hunter_dht
)

# Set include directories
target_include_directories(dht_hunter_bittorrent
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Install the library
install(TARGETS dht_hunter_bittorrent
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
