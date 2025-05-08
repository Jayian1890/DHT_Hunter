# Utility library
add_library(dht_hunter_utility
        # Common utilities
        common/common_utils.cpp

        # String utilities
        string/string_utils.cpp

        # Random utilities
        random/random_utils.cpp

        # Node utilities
        node/node_utils.cpp

        # Hash utilities
        hash/hash_utils.cpp

        # Filesystem utilities
        filesystem/filesystem_utils.cpp

        # File utilities
        file/file_utils.cpp

        # Thread utilities
        thread/thread_pool.cpp
        thread/thread_utils.cpp

        # Process utilities
        process/process_utils.cpp

        # System utilities
        system/memory_utils.cpp

        # Network utilities
        network/network_utils.cpp

        # JSON utilities
        json/json.cpp

        # Metadata utilities
        metadata/metadata_utils.cpp

        # Configuration utilities
        config/configuration_manager.cpp
)

# Set include directories
target_include_directories(dht_hunter_utility
        PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

# Set compiler options
target_compile_features(dht_hunter_utility
        PUBLIC
        cxx_std_20
)

# Link with required libraries
target_link_libraries(dht_hunter_utility
        PUBLIC
        dht_hunter_types
)

# Export the library
set_target_properties(dht_hunter_utility
        PROPERTIES
        EXPORT_NAME utility
)

# Install the library
install(TARGETS dht_hunter_utility
        EXPORT dht_hunter_targets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
)
