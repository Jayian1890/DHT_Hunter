# Add test executable
add_executable(template_test template_test.cpp)

# Link against GTest and the required libraries
target_link_libraries(template_test
    PRIVATE
    gtest
    gtest_main
    # Add your component libraries here
    # dht_hunter_your_component
    # dht_hunter_util
)

# Add the test to CTest
add_test(NAME template_test COMMAND template_test)
