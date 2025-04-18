#include <gtest/gtest.h>
#include "dht_hunter/dht/routing_table.hpp"

namespace dht_hunter::dht::test {

class RoutingTableTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate a random node ID for testing
        m_ownID = generateRandomNodeID();
        m_routingTable = std::make_unique<RoutingTable>(m_ownID);
    }
    
    NodeID m_ownID;
    std::unique_ptr<RoutingTable> m_routingTable;
};

TEST_F(RoutingTableTest, InitialState) {
    EXPECT_EQ(m_routingTable->getOwnID(), m_ownID);
    EXPECT_EQ(m_routingTable->size(), 0);
    EXPECT_TRUE(m_routingTable->isEmpty());
}

TEST_F(RoutingTableTest, AddNode) {
    // Create a node
    auto nodeID = generateRandomNodeID();
    auto endpoint = network::EndPoint("192.168.1.1:6881");
    auto node = std::make_shared<Node>(nodeID, endpoint);
    
    // Add the node
    EXPECT_TRUE(m_routingTable->addNode(node));
    
    // Check that the node was added
    EXPECT_EQ(m_routingTable->size(), 1);
    EXPECT_FALSE(m_routingTable->isEmpty());
    
    // Find the node
    auto foundNode = m_routingTable->findNode(nodeID);
    ASSERT_NE(foundNode, nullptr);
    EXPECT_EQ(foundNode->getID(), nodeID);
    EXPECT_EQ(foundNode->getEndpoint().toString(), endpoint.toString());
}

TEST_F(RoutingTableTest, RemoveNode) {
    // Create a node
    auto nodeID = generateRandomNodeID();
    auto endpoint = network::EndPoint("192.168.1.1:6881");
    auto node = std::make_shared<Node>(nodeID, endpoint);
    
    // Add the node
    EXPECT_TRUE(m_routingTable->addNode(node));
    
    // Remove the node
    EXPECT_TRUE(m_routingTable->removeNode(nodeID));
    
    // Check that the node was removed
    EXPECT_EQ(m_routingTable->size(), 0);
    EXPECT_TRUE(m_routingTable->isEmpty());
    
    // Try to find the node
    auto foundNode = m_routingTable->findNode(nodeID);
    EXPECT_EQ(foundNode, nullptr);
}

TEST_F(RoutingTableTest, GetClosestNodes) {
    // Create some nodes
    std::vector<std::shared_ptr<Node>> nodes;
    for (int i = 0; i < 10; i++) {
        auto nodeID = generateRandomNodeID();
        auto endpoint = network::EndPoint("192.168.1." + std::to_string(i + 1) + ":6881");
        auto node = std::make_shared<Node>(nodeID, endpoint);
        nodes.push_back(node);
        
        // Add the node
        EXPECT_TRUE(m_routingTable->addNode(node));
    }
    
    // Get the closest nodes to a random target
    auto targetID = generateRandomNodeID();
    auto closestNodes = m_routingTable->getClosestNodes(targetID, 5);
    
    // Check that we got the right number of nodes
    EXPECT_EQ(closestNodes.size(), 5);
    
    // Check that the nodes are sorted by distance
    for (size_t i = 1; i < closestNodes.size(); i++) {
        auto distanceA = calculateDistance(closestNodes[i - 1]->getID(), targetID);
        auto distanceB = calculateDistance(closestNodes[i]->getID(), targetID);
        
        // Check that distanceA <= distanceB
        EXPECT_LE(std::lexicographical_compare(
            distanceA.begin(), distanceA.end(),
            distanceB.begin(), distanceB.end()
        ), 0);
    }
}

TEST_F(RoutingTableTest, GetBucket) {
    // Create a node
    auto nodeID = generateRandomNodeID();
    auto endpoint = network::EndPoint("192.168.1.1:6881");
    auto node = std::make_shared<Node>(nodeID, endpoint);
    
    // Add the node
    EXPECT_TRUE(m_routingTable->addNode(node));
    
    // Get the bucket for the node
    auto& bucket = m_routingTable->getBucket(nodeID);
    
    // Check that the bucket contains the node
    auto foundNode = bucket.findNode(nodeID);
    ASSERT_NE(foundNode, nullptr);
    EXPECT_EQ(foundNode->getID(), nodeID);
    EXPECT_EQ(foundNode->getEndpoint().toString(), endpoint.toString());
}

TEST_F(RoutingTableTest, Clear) {
    // Create some nodes
    for (int i = 0; i < 10; i++) {
        auto nodeID = generateRandomNodeID();
        auto endpoint = network::EndPoint("192.168.1." + std::to_string(i + 1) + ":6881");
        auto node = std::make_shared<Node>(nodeID, endpoint);
        
        // Add the node
        EXPECT_TRUE(m_routingTable->addNode(node));
    }
    
    // Check that the nodes were added
    EXPECT_EQ(m_routingTable->size(), 10);
    
    // Clear the routing table
    m_routingTable->clear();
    
    // Check that the routing table is empty
    EXPECT_EQ(m_routingTable->size(), 0);
    EXPECT_TRUE(m_routingTable->isEmpty());
}

} // namespace dht_hunter::dht::test
