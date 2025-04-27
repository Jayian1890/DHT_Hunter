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
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Install the example
install(TARGETS unified_event_example
    RUNTIME DESTINATION bin/examples
)
