#include "dht_hunter/network/network_address.hpp"
#include "gtest/gtest.h"
#include <string>
#include <iostream>
#include <dht_hunter/logforge/logforge.hpp>

using namespace dht_hunter::network;

// Test NetworkAddress parsing and validation
TEST(AddressTest, ParsingAndValidation) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Test.AddressTest");
    logger->info("Testing address parsing and validation");

    // Test IPv4 address parsing
    NetworkAddress addr1("192.168.1.1");
    EXPECT_TRUE(addr1.isValid());
    EXPECT_EQ(addr1.getFamily(), AddressFamily::IPv4);
    EXPECT_EQ(addr1.toString(), "192.168.1.1");

    // Test IPv6 address parsing
    NetworkAddress addr2("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
    EXPECT_TRUE(addr2.isValid());
    EXPECT_EQ(addr2.getFamily(), AddressFamily::IPv6);
    EXPECT_EQ(addr2.toString(), "2001:db8:85a3::8a2e:370:7334"); // Normalized form

    // Test invalid address parsing
    NetworkAddress addr3("not-an-ip-address");
    EXPECT_FALSE(addr3.isValid());

    // Test special addresses
    NetworkAddress anyAddr = NetworkAddress::any(AddressFamily::IPv4);
    EXPECT_TRUE(anyAddr.isValid());
    EXPECT_EQ(anyAddr.getFamily(), AddressFamily::IPv4);
    EXPECT_EQ(anyAddr.toString(), "0.0.0.0");

    NetworkAddress loopbackAddr = NetworkAddress::loopback(AddressFamily::IPv4);
    EXPECT_TRUE(loopbackAddr.isValid());
    EXPECT_EQ(loopbackAddr.getFamily(), AddressFamily::IPv4);
    EXPECT_EQ(loopbackAddr.toString(), "127.0.0.1");

    // Test IPv6 any and loopback
    NetworkAddress anyAddrV6 = NetworkAddress::any(AddressFamily::IPv6);
    EXPECT_TRUE(anyAddrV6.isValid());
    EXPECT_EQ(anyAddrV6.getFamily(), AddressFamily::IPv6);
    EXPECT_EQ(anyAddrV6.toString(), "::");

    NetworkAddress loopbackAddrV6 = NetworkAddress::loopback(AddressFamily::IPv6);
    EXPECT_TRUE(loopbackAddrV6.isValid());
    EXPECT_EQ(loopbackAddrV6.getFamily(), AddressFamily::IPv6);
    EXPECT_EQ(loopbackAddrV6.toString(), "::1");
}

// Test EndPoint functionality
TEST(AddressTest, EndPointOperations) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Test.AddressTest");
    logger->info("Testing endpoint operations");

    // Create endpoints
    NetworkAddress addr1(std::string("192.168.1.1"));
    EndPoint ep1(addr1, 8080);
    EXPECT_TRUE(ep1.isValid());
    EXPECT_EQ(ep1.toString(), "192.168.1.1:8080");

    NetworkAddress addr2(std::string("2001:db8::1"));
    EndPoint ep2(addr2, 6881);
    EXPECT_TRUE(ep2.isValid());
    EXPECT_EQ(ep2.toString(), "[2001:db8::1]:6881");

    // Test endpoint equality
    NetworkAddress addr3(std::string("192.168.1.1"));
    EndPoint ep3(addr3, 8080);
    EXPECT_EQ(ep1, ep3);
    EXPECT_NE(ep1, ep2);

    // Test string constructor for endpoints
    EndPoint ep4(NetworkAddress(std::string("10.0.0.1")), 1234);
    EXPECT_TRUE(ep4.isValid());
    EXPECT_EQ(ep4.getAddress().toString(), "10.0.0.1");
    EXPECT_EQ(ep4.getPort(), 1234);

    // Test IPv6 string constructor
    EndPoint ep5(NetworkAddress(std::string("2001:db8::1")), 6881);
    EXPECT_TRUE(ep5.isValid());
    EXPECT_EQ(ep5.getAddress().toString(), "2001:db8::1");
    EXPECT_EQ(ep5.getPort(), 6881);

    // Test invalid address
    EndPoint ep6(NetworkAddress(std::string("not-an-endpoint")), 8080);
    EXPECT_FALSE(ep6.isValid());
}

// Required for GoogleTest
int main(int argc, char **argv) {
    // Initialize with synchronous logging (async=false)
    dht_hunter::logforge::LogForge::init(
        dht_hunter::logforge::LogLevel::INFO,
        dht_hunter::logforge::LogLevel::DEBUG,
        "address_test.log",
        true,  // useColors
        false  // async - set to false to avoid potential deadlocks
    );

    // Initialize GoogleTest
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}