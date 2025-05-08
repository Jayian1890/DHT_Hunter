# Add test executable for utils
add_executable(utils_tests
    common_utils_test.cpp
    system_utils_test.cpp
    domain_utils_test.cpp
)

# Link with dependencies
target_link_libraries(utils_tests
    PRIVATE
    dht_hunter_utility
    gtest
    gtest_main
)

# Add test to CTest
add_test(NAME utils_tests COMMAND utils_tests)
