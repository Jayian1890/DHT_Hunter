# Define the bencode library
add_library(dht_hunter_bencode
        bencode.cpp
)

# Set include directories
target_include_directories(dht_hunter_bencode
        PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_bencode
        PUBLIC
        cxx_std_17
)

# No external dependencies
# target_link_libraries(dht_hunter_bencode)
