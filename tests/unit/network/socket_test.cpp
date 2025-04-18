#include <gtest/gtest.h>
#include "dht_hunter/network/socket_factory.hpp"
#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/logforge/logforge.hpp"

namespace dht_hunter::network::test {

class SocketTest : public ::testing::Test {
protected:
    void SetUp() override {
        // No initialization needed for now
    }

    void TearDown() override {
        // Clean up
    }

    // No logger needed for now
};

TEST_F(SocketTest, CreateTCPSocket) {
    auto socket = SocketFactory::createTCPSocket();
    ASSERT_TRUE(socket != nullptr);
    EXPECT_TRUE(socket->isValid());
    EXPECT_EQ(socket->getType(), SocketType::TCP);
}

TEST_F(SocketTest, CreateUDPSocket) {
    auto socket = SocketFactory::createUDPSocket();
    ASSERT_TRUE(socket != nullptr);
    EXPECT_TRUE(socket->isValid());
    EXPECT_EQ(socket->getType(), SocketType::UDP);
}

TEST_F(SocketTest, NetworkAddressIPv4) {
    NetworkAddress addr("192.168.1.1");
    EXPECT_TRUE(addr.isValid());
    EXPECT_EQ(addr.getFamily(), AddressFamily::IPv4);
    EXPECT_EQ(addr.toString(), "192.168.1.1");

    // Test conversion to uint32_t
    uint32_t ipv4 = addr.getIPv4Address();
    EXPECT_EQ(ipv4, 0xC0A80101); // 192.168.1.1 in hex
}

TEST_F(SocketTest, NetworkAddressIPv6) {
    NetworkAddress addr("2001:db8::1");
    EXPECT_TRUE(addr.isValid());
    EXPECT_EQ(addr.getFamily(), AddressFamily::IPv6);
    EXPECT_EQ(addr.toString(), "2001:db8::1");
}

TEST_F(SocketTest, EndPoint) {
    EndPoint endpoint("192.168.1.1:8080");
    EXPECT_TRUE(endpoint.isValid());
    EXPECT_EQ(endpoint.getAddress().getFamily(), AddressFamily::IPv4);
    EXPECT_EQ(endpoint.getPort(), 8080);
    EXPECT_EQ(endpoint.toString(), "192.168.1.1:8080");
}

TEST_F(SocketTest, EndPointIPv6) {
    EndPoint endpoint("[2001:db8::1]:8080");
    EXPECT_TRUE(endpoint.isValid());
    EXPECT_EQ(endpoint.getAddress().getFamily(), AddressFamily::IPv6);
    EXPECT_EQ(endpoint.getPort(), 8080);
    EXPECT_EQ(endpoint.toString(), "[2001:db8::1]:8080");
}

TEST_F(SocketTest, LoopbackAddress) {
    NetworkAddress loopback = NetworkAddress::loopback();
    EXPECT_TRUE(loopback.isValid());
    EXPECT_TRUE(loopback.isLoopback());
    EXPECT_EQ(loopback.toString(), "127.0.0.1");

    NetworkAddress loopback6 = NetworkAddress::loopback(AddressFamily::IPv6);
    EXPECT_TRUE(loopback6.isValid());
    EXPECT_TRUE(loopback6.isLoopback());
    EXPECT_EQ(loopback6.toString(), "::1");
}

TEST_F(SocketTest, AnyAddress) {
    NetworkAddress any = NetworkAddress::any();
    EXPECT_TRUE(any.isValid());
    EXPECT_EQ(any.toString(), "0.0.0.0");

    NetworkAddress any6 = NetworkAddress::any(AddressFamily::IPv6);
    EXPECT_TRUE(any6.isValid());
    EXPECT_EQ(any6.toString(), "::");
}

} // namespace dht_hunter::network::test
