# Add the example executable
add_executable(logging_demo
    logging_demo.cpp
)

# Link with the logging library
target_link_libraries(logging_demo
    PRIVATE
    dht_hunter_logging
)

# Set include directories
target_include_directories(logging_demo
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
