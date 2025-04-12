# Define the bencode library
add_library(dht_hunter_bencode INTERFACE)

# Set include directories
target_include_directories(dht_hunter_bencode
    INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_bencode
    INTERFACE
    cxx_std_17
)
