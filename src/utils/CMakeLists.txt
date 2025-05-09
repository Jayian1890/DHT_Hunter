# Consolidated utilities module CMakeLists.txt

# Create the consolidated utilities library
add_library(bitscrape_utils
    common_utils.cpp
    domain_utils.cpp
    system_utils.cpp
    dht_core_utils.cpp
)

# Set include directories
target_include_directories(bitscrape_utils
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Set compile features
target_compile_features(bitscrape_utils
    PUBLIC
    cxx_std_23
)

# Install the library
install(TARGETS bitscrape_utils
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
