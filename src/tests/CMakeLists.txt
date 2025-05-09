# Tests for the consolidated utilities

# Create the test executable
add_executable(test_dht_core_utils
    test_dht_core_utils.cpp
)

# Set include directories
target_include_directories(test_dht_core_utils
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Link with the required libraries
target_link_libraries(test_dht_core_utils
    PRIVATE
    bitscrape_utils
    dht_hunter_unified_event
    dht_hunter_types
)

# Set compile features
target_compile_features(test_dht_core_utils
    PRIVATE
    cxx_std_23
)

# Add the test to CTest
add_test(NAME test_dht_core_utils COMMAND test_dht_core_utils)
