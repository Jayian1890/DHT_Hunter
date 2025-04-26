# Add bencode test executable
add_executable(bencode_test bencode_test.cpp)

# Link against GTest and the project libraries
target_link_libraries(bencode_test
    PRIVATE
    gtest
    gtest_main
    dht_hunter
)

# Add the test to CTest
add_test(NAME BencodeTest COMMAND bencode_test)
