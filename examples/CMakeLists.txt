# Add the unified event example
add_executable(unified_event_example unified_event_example.cpp)

# Link with the unified event library
target_link_libraries(unified_event_example
    PRIVATE
    dht_hunter_unified_event
)

# Set include directories
target_include_directories(unified_event_example
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Set C++ standard
set_target_properties(unified_event_example
    PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Install the example
install(TARGETS unified_event_example
    RUNTIME DESTINATION bin/examples
)

# Add the HTTPS client test
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

# Install the test
install(TARGETS https_client_test
    RUNTIME DESTINATION bin/examples
)
