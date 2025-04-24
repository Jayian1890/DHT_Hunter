#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <dht_hunter/logforge/logger_macros.hpp>

#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/log_initializer.hpp"
#include "dht_hunter/dht/dht_node.hpp"
#include "dht_hunter/crawler/infohash_collector.hpp"
#include "dht_hunter/util/filesystem_utils.hpp"

// Initialize logger before any other code runs
namespace {
    // This will be initialized during static initialization, before main() is called
    const auto& g_logInitializer = dht_hunter::logforge::LogInitializer::getInstance();
}

DEFINE_COMPONENT_LOGGER("Main", "Application")

// Global variables for signal handling
std::atomic<bool> g_running(true);

// Signal handler
void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

int main(int argc, char* argv[]) {
    std::string configDir = "config";

    // Initialize logging if not already initialized
    dht_hunter::logforge::getLogInitializer().initializeLogger(
        dht_hunter::logforge::LogLevel::TRACE,
        dht_hunter::logforge::LogLevel::TRACE,
        "dht_hunter.log",
        true,
        false
    );

    // Parse command line arguments
    uint16_t port = 6881;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--config" && i + 1 < argc) {
            configDir = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --port PORT      Port to listen on (default: 6881)" << std::endl;
            std::cout << "  --config DIR     Configuration directory (default: config)" << std::endl;
            std::cout << "  --help           Show this help message" << std::endl;
            return 0;
        }
    }

    // Ensure config directory exists
    if (!dht_hunter::util::FilesystemUtils::ensureDirectoryExists(configDir)) {
        std::cerr << "Failed to create config directory: " << configDir << std::endl;
        return 1;
    }

    auto logger = getLogger();

    // Register signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    logger->info("Starting DHT node on port {} with config directory {}", port, configDir);

    // Create and start the DHT node
    auto dhtNode = std::make_shared<dht_hunter::dht::DHTNode>(port, configDir);

    // Create an InfoHash collector
    auto collector = std::make_shared<dht_hunter::crawler::InfoHashCollector>();
    dhtNode->setInfoHashCollector(collector);

    // Start the DHT node
    if (!dhtNode->start()) {
        logger->error("Failed to start DHT node");
        return 1;
    }

    // Bootstrap the DHT node
    std::vector<std::string> bootstrapNodes = {
        "router.bittorrent.com:6881",
        "dht.transmissionbt.com:6881",
        "router.utorrent.com:6881"
    };

    logger->info("Bootstrapping DHT node with {} bootstrap nodes", bootstrapNodes.size());

    std::vector<dht_hunter::network::EndPoint> bootstrapEndpoints;
    for (const auto& node : bootstrapNodes) {
        // Parse host:port
        size_t colonPos = node.find(':');
        if (colonPos == std::string::npos) {
            logger->warning("Invalid bootstrap node format: {}", node);
            continue;
        }

        std::string host = node.substr(0, colonPos);
        uint16_t bootstrapPort = static_cast<uint16_t>(std::stoi(node.substr(colonPos + 1)));

        try {
            // Resolve the hostname
            auto addresses = dht_hunter::network::AddressResolver::resolve(host);
            if (!addresses.empty()) {
                bootstrapEndpoints.emplace_back(addresses[0], bootstrapPort);
                logger->info("Resolved bootstrap node {} to {}", node, bootstrapEndpoints.back().toString());
            } else {
                logger->warning("Failed to resolve bootstrap node: {}", host);
            }
        } catch (const std::exception& e) {
            logger->warning("Exception resolving bootstrap node {}: {}", host, e.what());
        }
    }

    // Bootstrap with the resolved endpoints
    if (!bootstrapEndpoints.empty()) {
        if (!dhtNode->bootstrap(bootstrapEndpoints)) {
            logger->warning("Failed to bootstrap DHT node");
        } else {
            logger->info("DHT node bootstrapped successfully");
        }
    } else {
        logger->warning("No valid bootstrap nodes found");
    }

    // Main loop
    logger->info("DHT node running, press Ctrl+C to exit");

    // Statistics variables
    size_t lastInfoHashCount = 0;
    auto lastStatsTime = std::chrono::steady_clock::now();

    while (g_running) {
        // Sleep for a short time
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Print statistics every minute
        auto now = std::chrono::steady_clock::now();
        if (now - lastStatsTime >= std::chrono::minutes(1)) {
            // Get routing table statistics
            size_t nodeCount = dhtNode->getRoutingTable().size();
            size_t bucketCount = dhtNode->getRoutingTable().getBuckets().size();

            // Get info hash statistics
            size_t infoHashCount = collector->getInfoHashCount();
            size_t newInfoHashes = infoHashCount - lastInfoHashCount;
            lastInfoHashCount = infoHashCount;

            // Log statistics
            logger->info("DHT Statistics: {} nodes in {} buckets, {} info hashes (+{} new)",
                        nodeCount, bucketCount, infoHashCount, newInfoHashes);

            // Perform a random lookup to keep the routing table fresh
            dht_hunter::dht::NodeID randomID = dht_hunter::dht::generateRandomNodeID();
            dhtNode->findClosestNodes(randomID, [logger](const std::vector<std::shared_ptr<dht_hunter::dht::Node>>& nodes) {
                logger->debug("Random lookup completed, found {} nodes", nodes.size());
            });

            lastStatsTime = now;
        }
    }

    // Stop the DHT node
    logger->info("Stopping DHT node");
    dhtNode->stop();

    // Save the routing table
    logger->info("Saving routing table");
    dhtNode->saveRoutingTable(configDir + "/routing_table.dat");

    // Flush logs before exit
    dht_hunter::logforge::LogForge::flush();

    logger->info("DHT node stopped");
    return 0;
}