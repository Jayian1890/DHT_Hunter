# Define the logforge library
add_library(dht_hunter_logforge
    logforge.cpp
)

# Set include directories
target_include_directories(dht_hunter_logforge
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_logforge
    PUBLIC
    cxx_std_20
)

# Install the library
install(TARGETS dht_hunter_logforge
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
