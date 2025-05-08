#include "utils/domain_utils.hpp"
#include <gtest/gtest.h>

namespace dht_hunter::utils::tests {

// Network utilities tests
TEST(NetworkUtilsTest, ParseHostPort) {
    auto [host, port] = network::parseHostPort("example.com:8080");
    EXPECT_EQ(host, "example.com");
    EXPECT_EQ(port, 8080);
    
    std::tie(host, port) = network::parseHostPort("192.168.1.1:80");
    EXPECT_EQ(host, "192.168.1.1");
    EXPECT_EQ(port, 80);
    
    std::tie(host, port) = network::parseHostPort("example.com");
    EXPECT_EQ(host, "example.com");
    EXPECT_EQ(port, 0);
    
    std::tie(host, port) = network::parseHostPort("example.com:invalid");
    EXPECT_EQ(host, "example.com");
    EXPECT_EQ(port, 0);
}

TEST(NetworkUtilsTest, FormatHostPort) {
    EXPECT_EQ(network::formatHostPort("example.com", 8080), "example.com:8080");
    EXPECT_EQ(network::formatHostPort("192.168.1.1", 80), "192.168.1.1:80");
}

TEST(NetworkUtilsTest, IsValidIpAddress) {
    EXPECT_TRUE(network::isValidIpAddress("192.168.1.1"));
    EXPECT_TRUE(network::isValidIpAddress("127.0.0.1"));
    EXPECT_TRUE(network::isValidIpAddress("8.8.8.8"));
    EXPECT_FALSE(network::isValidIpAddress("example.com"));
    EXPECT_FALSE(network::isValidIpAddress("256.256.256.256"));
    EXPECT_FALSE(network::isValidIpAddress("192.168.1"));
}

TEST(NetworkUtilsTest, IsPrivateIpAddress) {
    EXPECT_TRUE(network::isPrivateIpAddress("192.168.1.1"));
    EXPECT_TRUE(network::isPrivateIpAddress("10.0.0.1"));
    EXPECT_TRUE(network::isPrivateIpAddress("172.16.0.1"));
    EXPECT_TRUE(network::isPrivateIpAddress("127.0.0.1"));
    EXPECT_FALSE(network::isPrivateIpAddress("8.8.8.8"));
    EXPECT_FALSE(network::isPrivateIpAddress("203.0.113.1"));
}

TEST(NetworkUtilsTest, GetLocalIpAddress) {
    std::string localIp = network::getLocalIpAddress();
    EXPECT_FALSE(localIp.empty());
    EXPECT_TRUE(network::isValidIpAddress(localIp));
}

// Node utilities tests
TEST(NodeUtilsTest, GenerateRandomNodeID) {
    auto nodeId1 = node::generateRandomNodeID();
    auto nodeId2 = node::generateRandomNodeID();
    
    // The node IDs should be different
    EXPECT_NE(nodeId1, nodeId2);
}

TEST(NodeUtilsTest, CalculateDistance) {
    types::NodeID a({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
    types::NodeID b({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02});
    
    types::NodeID distance = node::calculateDistance(a, b);
    
    // The distance should be a XOR b
    types::NodeID expected({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03});
    EXPECT_EQ(distance, expected);
}

TEST(NodeUtilsTest, IsCloser) {
    types::NodeID target({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
    
    types::NodeID a({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
    
    types::NodeID b({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02});
    
    // a is closer to target than b
    EXPECT_TRUE(node::isCloser(a, b, target));
    EXPECT_FALSE(node::isCloser(b, a, target));
}

// Metadata utilities tests
TEST(MetadataUtilsTest, ValidateMetadata) {
    // Create a simple metadata
    std::vector<uint8_t> metadata = {'d', '4', ':', 'i', 'n', 'f', 'o', 'd', '4', ':', 'n', 'a', 'm', 'e', 
                                    '5', ':', 't', 'e', 's', 't', '1', 'e', 'e'};
    
    // Calculate the info hash
    types::InfoHash infoHash(hash::sha1(metadata));
    
    // Validate the metadata
    EXPECT_TRUE(metadata::validateMetadata(metadata, infoHash));
    
    // Modify the metadata
    metadata[10] = 'x';
    
    // Validation should fail
    EXPECT_FALSE(metadata::validateMetadata(metadata, infoHash));
}

TEST(MetadataUtilsTest, ExtractName) {
    // Create a simple metadata with a name
    std::vector<uint8_t> metadata = {'d', '4', ':', 'i', 'n', 'f', 'o', 'd', '4', ':', 'n', 'a', 'm', 'e', 
                                    '5', ':', 't', 'e', 's', 't', '1', 'e', 'e'};
    
    // Extract the name
    auto result = metadata::extractName(metadata);
    EXPECT_TRUE(result.isSuccess());
    EXPECT_EQ(result.getValue(), "test1");
}

} // namespace dht_hunter::utils::tests
