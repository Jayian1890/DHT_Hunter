# Types module CMakeLists.txt
add_library(dht_hunter_types
    node_id.cpp
    info_hash.cpp
    endpoint.cpp
    dht_types.cpp
    message_types.cpp
    event_types.cpp
)

# Set include directories
target_include_directories(dht_hunter_types
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Link with required libraries
target_link_libraries(dht_hunter_types
    PUBLIC
    dht_hunter_core
    fmt::fmt
)

# Install the library
install(TARGETS dht_hunter_types
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
