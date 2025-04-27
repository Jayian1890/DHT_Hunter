# Define the unified event library
add_library(dht_hunter_unified_event
        # Core components
        event.cpp
        event_bus.cpp

        # Processors
        processors/logging_processor.cpp
        processors/component_processor.cpp
        processors/statistics_processor.cpp

        # Adapters
        adapters/dht_event_adapter.cpp
)

# Set include directories
target_include_directories(dht_hunter_unified_event
        PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

# Link against required libraries
target_link_libraries(dht_hunter_unified_event
        PUBLIC
        dht_hunter_dht
        dht_hunter_network
)

# Set C++ standard
set_target_properties(dht_hunter_unified_event
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
)

# Install the library
install(TARGETS dht_hunter_unified_event
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)

# Install the headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/dht_hunter/unified_event
        DESTINATION include/dht_hunter
)
