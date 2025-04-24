# Define the util library
add_library(dht_hunter_util
    hash.cpp
)

# Set include directories
target_include_directories(dht_hunter_util
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_util
    PUBLIC
    cxx_std_17
)

# Link with the logging library
target_link_libraries(dht_hunter_util
    PUBLIC
    dht_hunter_logforge
)
