#include "dht_hunter/dht/core/dht_node.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/event/logger.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

using namespace dht_hunter;

std::atomic<bool> running(true);

void signalHandler(int signal) {
    std::cout << "Received signal " << signal << std::endl;
    running = false;
}

int main(int argc, char** argv) {
    // Initialize logging
    logforge::LogForge::getInstance().init(
        logforge::LogLevel::DEBUG,
        logforge::LogLevel::DEBUG,
        "dht_test.log",
        true,
        false
    );
    
    // Create a logger
    auto logger = event::Logger::forComponent("Main");
    
    // Register signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Parse command line arguments
    uint16_t port = 6881;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::stoi(argv[1]));
    }
    
    logger.info("Starting DHT test on port {}", port);
    
    // Create a DHT configuration
    dht::DHTConfig config(port);
    
    // Create a DHT node
    auto dhtNode = std::make_shared<dht::DHTNode>(config);
    
    // Start the DHT node
    if (!dhtNode->start()) {
        logger.error("Failed to start DHT node");
        return 1;
    }
    
    logger.info("DHT node started with ID: {}", dht::nodeIDToString(dhtNode->getNodeID()));
    
    // Wait for the node to bootstrap
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // Perform a find_node lookup for a random node ID
    dht::NodeID randomID = dht::generateRandomNodeID();
    logger.info("Performing find_node lookup for random node ID: {}", dht::nodeIDToString(randomID));
    
    dhtNode->findClosestNodes(randomID, [&logger](const std::vector<std::shared_ptr<dht::Node>>& nodes) {
        logger.info("Found {} nodes", nodes.size());
        
        for (const auto& node : nodes) {
            logger.info("  Node: {} at {}", dht::nodeIDToString(node->getID()), node->getEndpoint().toString());
        }
    });
    
    // Wait for the lookup to complete
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // Perform a get_peers lookup for a random info hash
    dht::InfoHash randomInfoHash = dht::generateRandomNodeID();
    logger.info("Performing get_peers lookup for random info hash: {}", dht::infoHashToString(randomInfoHash));
    
    dhtNode->getPeers(randomInfoHash, [&logger](const std::vector<network::EndPoint>& peers) {
        logger.info("Found {} peers", peers.size());
        
        for (const auto& peer : peers) {
            logger.info("  Peer: {}", peer.toString());
        }
    });
    
    // Wait for the lookup to complete
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // Announce a peer
    logger.info("Announcing peer for random info hash: {}", dht::infoHashToString(randomInfoHash));
    
    dhtNode->announcePeer(randomInfoHash, port, [&logger](bool success) {
        if (success) {
            logger.info("Peer announced successfully");
        } else {
            logger.error("Failed to announce peer");
        }
    });
    
    // Wait for the announcement to complete
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // Main loop
    logger.info("Entering main loop");
    
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // Stop the DHT node
    logger.info("Stopping DHT node");
    dhtNode->stop();
    
    logger.info("DHT test completed");
    
    return 0;
}
