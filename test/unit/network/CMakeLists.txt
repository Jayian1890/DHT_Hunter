# Define common dependencies for all network tests
set(NETWORK_TEST_DEPS
    dht_hunter_network
    dht_hunter_util
    gtest
    gtest_main
)

# Socket tests
add_executable(socket_test socket_test.cpp)
target_link_libraries(socket_test PRIVATE ${NETWORK_TEST_DEPS})
add_test(NAME socket_test COMMAND socket_test)

# Address and endpoint tests
add_executable(address_test address_test.cpp)
target_link_libraries(address_test PRIVATE ${NETWORK_TEST_DEPS})
add_test(NAME address_test COMMAND address_test)

# Buffer management tests
add_executable(buffer_test buffer_test.cpp)
target_link_libraries(buffer_test PRIVATE ${NETWORK_TEST_DEPS})
add_test(NAME buffer_test COMMAND buffer_test)

# Rate limiter tests
add_executable(rate_limiter_test rate_limiter_test.cpp)
target_link_libraries(rate_limiter_test PRIVATE ${NETWORK_TEST_DEPS})
add_test(NAME rate_limiter_test COMMAND rate_limiter_test)