# Define the storage library
add_library(dht_hunter_storage
    metadata_storage.cpp
)

# Set include directories
target_include_directories(dht_hunter_storage
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Link with core library only
target_link_libraries(dht_hunter_storage
    PUBLIC
    dht_hunter_core
)
