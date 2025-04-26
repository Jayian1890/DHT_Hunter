# Define the event library
add_library(dht_hunter_event
    event_system.cpp
)

# Set include directories
target_include_directories(dht_hunter_event
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_event
    PUBLIC
    cxx_std_17
)

# No external dependencies
# target_link_libraries(dht_hunter_event)
