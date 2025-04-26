# Define the service library
add_library(dht_hunter_service
    service_locator.cpp
)

# Set include directories
target_include_directories(dht_hunter_service
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_service
    PUBLIC
    cxx_std_17
)

# No external dependencies
# target_link_libraries(dht_hunter_service)
