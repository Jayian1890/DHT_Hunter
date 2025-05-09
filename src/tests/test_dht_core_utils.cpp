#include "utils/dht_core_utils.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

using namespace dht_hunter::utils::dht_core;
using namespace dht_hunter::types;
using namespace dht_hunter::network;

/**
 * @brief Test the DHTConfig class
 * @return True if all tests pass, false otherwise
 */
bool testDHTConfig() {
    std::cout << "Testing DHTConfig..." << std::endl;

    // Create a config with default values
    DHTConfig config;

    // Check default values
    if (config.getPort() != 6881) {
        std::cerr << "Default port is not 6881" << std::endl;
        return false;
    }

    if (config.getKBucketSize() != K_BUCKET_SIZE) {
        std::cerr << "Default k-bucket size is not " << K_BUCKET_SIZE << std::endl;
        return false;
    }

    // Set new values
    config.setPort(6882);
    config.setKBucketSize(32);

    // Check new values
    if (config.getPort() != 6882) {
        std::cerr << "Port was not set to 6882" << std::endl;
        return false;
    }

    if (config.getKBucketSize() != 32) {
        std::cerr << "K-bucket size was not set to 32" << std::endl;
        return false;
    }

    std::cout << "DHTConfig tests passed!" << std::endl;
    return true;
}

/**
 * @brief Test the RoutingTable class
 * @return True if all tests pass, false otherwise
 */
bool testRoutingTable() {
    std::cout << "Testing RoutingTable..." << std::endl;

    // Create a routing table
    NodeID nodeID = NodeID::random();
    auto routingTable = RoutingTable::getInstance(nodeID);

    // Check initial state
    if (routingTable->getNodeCount() != 0) {
        std::cerr << "Initial node count is not 0" << std::endl;
        return false;
    }

    if (routingTable->getBucketCount() != 1) {
        std::cerr << "Initial bucket count is not 1" << std::endl;
        return false;
    }

    // Add some nodes
    for (int i = 0; i < 10; ++i) {
        NodeID id = NodeID::random();
        NetworkAddress address("192.168.1." + std::to_string(i + 1));
        EndPoint endpoint(address, 6881);
        auto node = std::make_shared<Node>(id, endpoint);
        routingTable->addNode(node);
    }

    // Check that nodes were added
    if (routingTable->getNodeCount() == 0) {
        std::cerr << "No nodes were added to the routing table" << std::endl;
        return false;
    }

    // Get closest nodes to a target
    NodeID targetID = NodeID::random();
    auto closestNodes = routingTable->getClosestNodes(targetID, 5);

    // Check that we got the requested number of nodes (or fewer if there aren't enough)
    if (closestNodes.size() > 5) {
        std::cerr << "Got more than 5 closest nodes" << std::endl;
        return false;
    }

    std::cout << "RoutingTable tests passed!" << std::endl;
    return true;
}

/**
 * @brief Test the DHTNode class
 * @return True if all tests pass, false otherwise
 */
bool testDHTNode() {
    std::cout << "Testing DHTNode..." << std::endl;

    // Create a DHT node
    DHTConfig config;
    NodeID nodeID = NodeID::random();
    auto dhtNode = DHTNode::getInstance(config, nodeID);

    // Check initial state
    if (dhtNode->isRunning()) {
        std::cerr << "DHT node is running before start() is called" << std::endl;
        return false;
    }

    // Start the node
    if (!dhtNode->start()) {
        std::cerr << "Failed to start DHT node" << std::endl;
        return false;
    }

    // Check that the node is running
    if (!dhtNode->isRunning()) {
        std::cerr << "DHT node is not running after start() is called" << std::endl;
        return false;
    }

    // Stop the node
    dhtNode->stop();

    // Check that the node is stopped
    if (dhtNode->isRunning()) {
        std::cerr << "DHT node is still running after stop() is called" << std::endl;
        return false;
    }

    std::cout << "DHTNode tests passed!" << std::endl;
    return true;
}

/**
 * @brief Test the PersistenceManager class
 * @return True if all tests pass, false otherwise
 */
bool testPersistenceManager() {
    std::cout << "Testing PersistenceManager..." << std::endl;

    // Create the test_config directory
    std::string testConfigDir = "./test_config";
    std::filesystem::create_directories(testConfigDir);

    // Create a persistence manager
    auto persistenceManager = PersistenceManager::getInstance(testConfigDir);

    // Save and load a node ID
    NodeID nodeID = NodeID::random();
    if (!persistenceManager->saveNodeID(nodeID)) {
        std::cerr << "Failed to save node ID" << std::endl;
        return false;
    }

    NodeID loadedNodeID = persistenceManager->loadNodeID();
    if (loadedNodeID != nodeID) {
        std::cerr << "Loaded node ID does not match saved node ID" << std::endl;
        return false;
    }

    std::cout << "PersistenceManager tests passed!" << std::endl;
    return true;
}

/**
 * @brief Main function
 * @return 0 if all tests pass, 1 otherwise
 */
int main() {
    // Initialize the event system
    dht_hunter::unified_event::initializeEventSystem();

    // Run the tests
    bool allTestsPassed = true;

    allTestsPassed &= testDHTConfig();
    allTestsPassed &= testRoutingTable();
    allTestsPassed &= testDHTNode();
    allTestsPassed &= testPersistenceManager();

    if (allTestsPassed) {
        std::cout << "All tests passed!" << std::endl;
        return 0;
    } else {
        std::cerr << "Some tests failed!" << std::endl;
        return 1;
    }
}
