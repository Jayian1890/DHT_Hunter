#include <gtest/gtest.h>
#include "dht_hunter/network/network_address.hpp"

namespace dht_hunter::network::test {

TEST(SimpleTest, NetworkAddressIPv4) {
    NetworkAddress addr("192.168.1.1");
    EXPECT_TRUE(addr.isValid());
    EXPECT_EQ(addr.getFamily(), AddressFamily::IPv4);
    EXPECT_EQ(addr.toString(), "192.168.1.1");
}

} // namespace dht_hunter::network::test
