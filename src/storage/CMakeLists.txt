# Define the storage library
add_library(dht_hunter_storage INTERFACE)

# Set include directories
target_include_directories(dht_hunter_storage
    INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_storage
    INTERFACE
    cxx_std_17
)
