# Utility library
add_library(dht_hunter_util
    filesystem_utils.cpp
    hash.cpp
    process_utils.cpp
)

# Set include directories
target_include_directories(dht_hunter_util
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Link dependencies
target_link_libraries(dht_hunter_util
    PUBLIC
    dht_hunter_core
    dht_hunter_logforge
)

# Install the library
install(TARGETS dht_hunter_util
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
