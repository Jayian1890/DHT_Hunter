# Define the logging library
add_library(dht_hunter_logging
    logger.cpp
)

# Set include directories
target_include_directories(dht_hunter_logging
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_logging
    PUBLIC
    cxx_std_17
)

# Install the library
install(TARGETS dht_hunter_logging
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
