# Define the core library
add_library(dht_hunter_core_impl
    application_controller.cpp
)

# Set include directories
target_include_directories(dht_hunter_core_impl
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Link with required libraries
target_link_libraries(dht_hunter_core_impl
    PUBLIC
    dht_hunter_types
    dht_hunter_utility
    dht_hunter_network
    dht_hunter_dht
    dht_hunter_bittorrent
    dht_hunter_unified_event
    dht_hunter_web
)

# Install the library
install(TARGETS dht_hunter_core_impl
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
