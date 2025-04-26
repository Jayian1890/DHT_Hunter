# Define the metadata library
add_library(dht_hunter_metadata
    bt_handshake.cpp
    extension_protocol.cpp
    ut_metadata.cpp
    bt_connection.cpp
    torrent_file.cpp
    metadata_fetcher.cpp
    torrent_file_builder.cpp
)

# Set include directories
target_include_directories(dht_hunter_metadata
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(dht_hunter_metadata
    PUBLIC
    cxx_std_17
)

# No external dependencies
# target_link_libraries(dht_hunter_metadata)
