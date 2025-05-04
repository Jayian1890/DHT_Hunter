# Define the web library
add_library(dht_hunter_web
    web_server.cpp
    web_bundle_manager.cpp
    api/config_api_handler.cpp
)

# Set include directories
target_include_directories(dht_hunter_web
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_web
    PUBLIC
    cxx_std_20
)

# Link with required libraries
target_link_libraries(dht_hunter_web
    PUBLIC
    dht_hunter_network
    dht_hunter_dht
    dht_hunter_unified_event
    dht_hunter_utility
)

# Install the library
install(TARGETS dht_hunter_web
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Note: Web files are bundled at the top level CMakeLists.txt
