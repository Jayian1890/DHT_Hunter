# DHT tests

# Message test
add_executable(message_test
    message_test.cpp
)

target_link_libraries(message_test
    PRIVATE
    dht_hunter_dht
    gtest
    gtest_main
)

add_test(NAME message_test COMMAND message_test)

# Routing table test
add_executable(routing_table_test
    routing_table_test.cpp
)

target_link_libraries(routing_table_test
    PRIVATE
    dht_hunter_dht
    gtest
    gtest_main
)

add_test(NAME routing_table_test COMMAND routing_table_test)

# DHT node test
add_executable(dht_node_test
    dht_node_test.cpp
)

target_link_libraries(dht_node_test
    PRIVATE
    dht_hunter_dht
    gtest
    gtest_main
)

add_test(NAME dht_node_test COMMAND dht_node_test)
