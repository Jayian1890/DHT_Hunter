# Network tests

# Socket test
add_executable(socket_test
    socket_test.cpp
)

target_link_libraries(socket_test
    PRIVATE
    dht_hunter_network
    gtest
    gtest_main
)

add_test(NAME socket_test COMMAND socket_test)

# Simple test
add_executable(simple_test
    simple_test.cpp
)

target_link_libraries(simple_test
    PRIVATE
    dht_hunter_network
    gtest
    gtest_main
)

add_test(NAME simple_test COMMAND simple_test)
