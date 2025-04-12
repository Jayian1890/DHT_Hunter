# Define the crawler library
add_library(dht_hunter_crawler INTERFACE)

# Set include directories
target_include_directories(dht_hunter_crawler
    INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_crawler
    INTERFACE
    cxx_std_17
)
