# Define the metadata library
add_library(dht_hunter_metadata INTERFACE)

# Set include directories
target_include_directories(dht_hunter_metadata
    INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_metadata
    INTERFACE
    cxx_std_17
)
