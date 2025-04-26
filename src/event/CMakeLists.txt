# Define the event library
add_library(dht_hunter_event INTERFACE)

# Set include directories
target_include_directories(dht_hunter_event
    INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_event
    INTERFACE
    cxx_std_20
)

# Find the fmt library
find_package(fmt REQUIRED)

# Link with the logging library and fmt
target_link_libraries(dht_hunter_event
    INTERFACE
    dht_hunter_logforge
    fmt::fmt
)
