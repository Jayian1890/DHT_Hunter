# Add the logforge test executable
add_executable(logforge_test
    logforge_test.cpp
)

# Link with the logforge library and Google Test
target_link_libraries(logforge_test
    PRIVATE
    dht_hunter_logforge
    gtest
    gtest_main
)

# Add the test to CTest
add_test(NAME LogForgeTest COMMAND logforge_test)

# Add the formatter test executable
add_executable(formatter_test
    formatter_test.cpp
)

# Link with the logforge library and Google Test
target_link_libraries(formatter_test
    PRIVATE
    dht_hunter_logforge
    gtest
    gtest_main
)

# Add the test to CTest
add_test(NAME FormatterTest COMMAND formatter_test)

# Add the async logger test executable
add_executable(async_logforge_test
    async_logforge_test.cpp
)

# Link with the logforge library and Google Test
target_link_libraries(async_logforge_test
    PRIVATE
    dht_hunter_logforge
    gtest
    gtest_main
)

# Add the test to CTest
add_test(NAME AsyncLogForgeTest COMMAND async_logforge_test)

# Add the rotating file sink test executable
add_executable(rotating_file_sink_test
    rotating_file_sink_test.cpp
)

# Link with the logforge library and Google Test
target_link_libraries(rotating_file_sink_test
    PRIVATE
    dht_hunter_logforge
    gtest
    gtest_main
)

# Add the test to CTest
add_test(NAME RotatingFileSinkTest COMMAND rotating_file_sink_test)
