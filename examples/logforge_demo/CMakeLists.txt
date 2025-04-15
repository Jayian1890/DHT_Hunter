# Add the logforge demo executable
add_executable(logforge_demo
    logforge_demo.cpp
)

# Link with the logforge library
target_link_libraries(logforge_demo
    PRIVATE
    dht_hunter_logforge
)

# Set include directories
target_include_directories(logforge_demo
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Add the formatter demo executable
add_executable(formatter_demo
    formatter_demo.cpp
)

# Link with the logforge library
target_link_libraries(formatter_demo
    PRIVATE
    dht_hunter_logforge
)

# Set include directories
target_include_directories(formatter_demo
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Add the async logging demo executable
add_executable(async_logging_demo
    async_logging_demo.cpp
)

# Link with the logforge library
target_link_libraries(async_logging_demo
    PRIVATE
    dht_hunter_logforge
)

# Set include directories
target_include_directories(async_logging_demo
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Add the async test executable
add_executable(async_test
    async_test.cpp
)

# Link with the logforge library
target_link_libraries(async_test
    PRIVATE
    dht_hunter_logforge
)

# Set include directories
target_include_directories(async_test
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Add the rotation demo executable
add_executable(rotation_demo
    rotation_demo.cpp
)

# Link with the logforge library
target_link_libraries(rotation_demo
    PRIVATE
    dht_hunter_logforge
)

# Set include directories
target_include_directories(rotation_demo
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
