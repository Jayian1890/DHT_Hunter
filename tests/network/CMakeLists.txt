# Network tests

# HTTPS client test
add_executable(https_client_test https_client_test.cpp)

# Link with the required libraries
target_link_libraries(https_client_test
    PRIVATE
    dht_hunter_network
    dht_hunter_unified_event
)

# Set include directories
target_include_directories(https_client_test
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Set C++ standard
set_target_properties(https_client_test
    PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Add the test to CTest
add_test(
    NAME HttpsClientTest
    COMMAND https_client_test
)

# Set timeout for the test (2 minutes)
set_tests_properties(HttpsClientTest PROPERTIES TIMEOUT 120)
