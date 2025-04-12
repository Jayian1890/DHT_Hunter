# Add the test executable
add_executable(logger_test
    logger_test.cpp
)

# Link with the logging library and Google Test
target_link_libraries(logger_test
    PRIVATE
    dht_hunter_logging
    gtest
    gtest_main
)

# Add the test to CTest
add_test(NAME LoggerTest COMMAND logger_test)
