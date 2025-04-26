# Add the DHT test executable
add_executable(dht_test dht_test.cpp)

# Link with the DHT library
target_link_libraries(dht_test
    PRIVATE
    dht_hunter_dht
)

# Set include directories
target_include_directories(dht_test
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Install the executable
install(TARGETS dht_test
    RUNTIME DESTINATION bin
)
