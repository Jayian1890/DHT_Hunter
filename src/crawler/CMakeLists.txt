# Define the crawler library
add_library(dht_hunter_crawler
    infohash_collector.cpp
    dht_crawler.cpp
)

# Set include directories
target_include_directories(dht_hunter_crawler
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_crawler
    PUBLIC
    cxx_std_17
)

# Link with other libraries
target_link_libraries(dht_hunter_crawler
    PUBLIC
    dht_hunter_logforge
    dht_hunter_util
    dht_hunter_dht
)
