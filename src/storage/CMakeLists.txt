# Define the storage library
add_library(dht_hunter_storage
    metadata_storage.cpp
)

# Set include directories
target_include_directories(dht_hunter_storage
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_storage
    PUBLIC
    cxx_std_17
)

# Link with other libraries
target_link_libraries(dht_hunter_storage
    PUBLIC
    dht_hunter_core
    PRIVATE
    dht_hunter_logforge
    dht_hunter_util
    dht_hunter_metadata
)

# Create test executables
add_executable(test_metadata_storage
    test_metadata_storage.cpp
)

# Link dependencies for tests
target_link_libraries(test_metadata_storage
    PRIVATE
    dht_hunter_storage
    dht_hunter_logforge
)

# Set include directories for tests
target_include_directories(test_metadata_storage
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
