# Utility library (legacy components)
add_library(dht_hunter_utility
        # Common utilities
        common/common_utils.cpp

        # Random utilities
        random/random_utils.cpp

        # Thread utilities
        thread/thread_pool.cpp
        thread/thread_utils.cpp

        # Process utilities
        process/process_utils.cpp

        # System utilities
        system/memory_utils.cpp

        # JSON utilities
        json/json.cpp

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
        bitscrape_utils
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
