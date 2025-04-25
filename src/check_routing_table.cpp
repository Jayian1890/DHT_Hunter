#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/log_initializer.hpp"
#include "dht_hunter/dht/dht_node.hpp"
#include "dht_hunter/network/network_address.hpp"
#include "routing_table_check.cpp"

// Global variables for signal handling
std::atomic<bool> g_running(true);
std::shared_ptr<dht_hunter::dht::DHTNode> g_dhtNode;

// Signal handler
void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    g_running.store(false);
}

int main(int argc, char* argv[]) {
    // Initialize logging
    dht_hunter::logforge::getLogInitializer().initializeLogger(
        dht_hunter::logforge::LogLevel::INFO,  // Console level
        dht_hunter::logforge::LogLevel::INFO,  // File level
        "routing_table_check",  // Log file name
        true,  // Use colors
        false  // Async logging disabled
    );

    auto logger = dht_hunter::logforge::LogForge::getLogger("Main");

    // Register signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Parse command line arguments
    uint16_t port = 6881;
    if (argc > 1) {
        try {
            port = static_cast<uint16_t>(std::stoi(argv[1]));
        } catch (const std::exception& e) {
            logger->error("Invalid port number: {}", e.what());
            return 1;
        }
    }

    // Create DHT node
    logger->info("Creating DHT node on port {}", port);
    g_dhtNode = std::make_shared<dht_hunter::dht::DHTNode>(port, "config");

    // Start DHT node
    logger->info("Starting DHT node...");
    if (!g_dhtNode->start()) {
        logger->error("Failed to start DHT node");
        return 1;
    }

    // Bootstrap with well-known nodes
    logger->info("Bootstrapping with well-known nodes...");
    std::vector<std::string> bootstrapNodes = {
        "router.bittorrent.com:6881",
        "dht.transmissionbt.com:6881",
        "router.utorrent.com:6881",
        "router.bitcomet.com:6881",
        "dht.aelitis.com:6881"
    };

    std::vector<dht_hunter::network::EndPoint> endpoints;
    for (const auto& node : bootstrapNodes) {
        size_t colonPos = node.find(':');
        if (colonPos != std::string::npos) {
            std::string host = node.substr(0, colonPos);
            uint16_t nodePort = static_cast<uint16_t>(std::stoi(node.substr(colonPos + 1)));
            try {
                auto addresses = dht_hunter::network::AddressResolver::resolve(host);
                if (!addresses.empty()) {
                    endpoints.emplace_back(addresses[0], nodePort);
                    logger->info("Resolved {} to {}", node, addresses[0].toString());
                }
            } catch (const std::exception& e) {
                logger->warning("Failed to resolve {}: {}", node, e.what());
            }
        }
    }

    if (!endpoints.empty()) {
        if (g_dhtNode->bootstrap(endpoints)) {
            logger->info("Successfully bootstrapped with at least one node");
        } else {
            logger->warning("Failed to bootstrap with any nodes");
        }
    } else {
        logger->error("No bootstrap nodes could be resolved");
    }

    logger->info("DHT node started successfully");

    // Wait for the DHT node to bootstrap and build its routing table
    logger->info("Waiting 30 seconds for the DHT node to bootstrap...");
    for (int i = 0; i < 30 && g_running; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (i % 5 == 0) {
            logger->info("Waiting... {} seconds elapsed", i);
        }
    }

    if (!g_running) {
        logger->info("Shutdown requested during bootstrap wait");
        g_dhtNode->stop();
        return 0;
    }

    // Check routing table health
    logger->info("Checking routing table health...");
    dht_hunter::diagnostic::checkRoutingTableHealth(g_dhtNode);

    // Stop DHT node
    logger->info("Stopping DHT node...");
    g_dhtNode->stop();

    logger->info("Routing table check completed");
    return 0;
}
