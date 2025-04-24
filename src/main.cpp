#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <vector>
#include <algorithm>
#include <functional>
#include <iomanip>
#include <filesystem>
#include <limits>
#include <dht_hunter/logforge/logger_macros.hpp>

#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/log_initializer.hpp"
#include "dht_hunter/dht/dht_node.hpp"
#include "dht_hunter/crawler/infohash_collector.hpp"
#include "dht_hunter/util/filesystem_utils.hpp"

namespace {
    // This struct will be initialized during static initialization,
    struct LogInitializer {
        LogInitializer() {
            dht_hunter::logforge::getLogInitializer().initializeLogger(
                dht_hunter::logforge::LogLevel::TRACE,  // Console level
                dht_hunter::logforge::LogLevel::TRACE,  // File level
                "",  // Empty string means use executable name for log file
                true,  // Use colors
                false  // Async logging disabled
            );
        }
    };

    // This static instance will be initialized before main() is called
    [[maybe_unused]] LogInitializer g_logInitializer;
}

DEFINE_COMPONENT_LOGGER("Main", "Application")

// Global variables for signal handling
std::atomic<bool> g_running(true);
std::atomic<bool> g_shutdown_requested(false);
std::shared_ptr<dht_hunter::dht::DHTNode> g_dhtNode;
std::shared_ptr<dht_hunter::crawler::InfoHashCollector> g_collector;

// Signal handler
void signalHandler(int signal) {
    if (g_shutdown_requested.load()) {
        // If shutdown was already requested, force exit
        std::cerr << "Forced exit due to second signal " << signal << std::endl;
        std::exit(1);
    }

    std::cout << "Received signal " << signal << ", shutting down gracefully..." << std::endl;
    g_shutdown_requested.store(true);
    g_running.store(false);
}

/**
 * @brief Structure to hold command line arguments
 */
struct CommandLineArgs {
    uint16_t port = 6881;
    std::string configDir = "config";
    bool verbose = false;
    size_t maxInfoHashes = 1000000;
    std::chrono::seconds saveInterval = std::chrono::seconds(300);
};

/**
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @return Parsed arguments
 */
CommandLineArgs parseCommandLine(int argc, char* argv[]) {
    CommandLineArgs args;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            args.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--config" && i + 1 < argc) {
            args.configDir = argv[++i];
        } else if (arg == "--verbose" || arg == "-v") {
            args.verbose = true;
        } else if (arg == "--max-infohashes" && i + 1 < argc) {
            args.maxInfoHashes = static_cast<size_t>(std::stoull(argv[++i]));
        } else if (arg == "--save-interval" && i + 1 < argc) {
            args.saveInterval = std::chrono::seconds(std::stoi(argv[++i]));
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --port PORT              Port to listen on (default: 6881)" << std::endl;
            std::cout << "  --config DIR             Configuration directory (default: config)" << std::endl;
            std::cout << "  --verbose, -v            Enable verbose logging" << std::endl;
            std::cout << "  --max-infohashes NUM     Maximum number of infohashes to store (default: 1000000)" << std::endl;
            std::cout << "  --save-interval SEC     Interval in seconds for saving infohashes (default: 300)" << std::endl;
            std::cout << "  --help, -h               Show this help message" << std::endl;
            std::exit(0);
        }
    }

    return args;
}

/**
 * @brief Configure the InfoHashCollector
 * @param collector The collector to configure
 * @param args Command line arguments
 * @param configDir Configuration directory
 */
void configureInfoHashCollector(std::shared_ptr<dht_hunter::crawler::InfoHashCollector> collector,
                               const CommandLineArgs& args,
                               const std::string& configDir) {
    dht_hunter::crawler::InfoHashCollectorConfig config;
    config.maxStoredInfoHashes = static_cast<uint32_t>(std::min(args.maxInfoHashes, static_cast<size_t>(std::numeric_limits<uint32_t>::max())));
    config.saveInterval = args.saveInterval;
    config.savePath = configDir + "/infohashes.dat";
    config.deduplicateInfoHashes = true;
    config.filterInvalidInfoHashes = true;
    config.maxQueueSize = 100000; // Increased queue size for better performance

    collector->setConfig(config);
    collector->start();

    // Try to load existing infohashes
    if (std::filesystem::exists(config.savePath)) {
        getLogger()->info("Loading infohashes from {}", config.savePath);
        collector->loadInfoHashes(config.savePath);
    }
}

/**
 * @brief Bootstrap the DHT node with well-known bootstrap nodes
 * @param dhtNode The DHT node to bootstrap
 * @return True if bootstrapped successfully, false otherwise
 */
bool bootstrapDHTNode(std::shared_ptr<dht_hunter::dht::DHTNode> dhtNode) {
    // List of well-known bootstrap nodes
    std::vector<std::string> bootstrapNodes = {
        "router.bittorrent.com:6881",
        "dht.transmissionbt.com:6881",
        "router.utorrent.com:6881",
        "router.bitcomet.com:6881",
        "dht.aelitis.com:6881"
    };

    getLogger()->info("Bootstrapping DHT node with {} bootstrap nodes", bootstrapNodes.size());

    std::vector<dht_hunter::network::EndPoint> bootstrapEndpoints;
    for (const auto& node : bootstrapNodes) {
        // Parse host:port
        size_t colonPos = node.find(':');
        if (colonPos == std::string::npos) {
            getLogger()->warning("Invalid bootstrap node format: {}", node);
            continue;
        }

        std::string host = node.substr(0, colonPos);
        uint16_t bootstrapPort;

        try {
            bootstrapPort = static_cast<uint16_t>(std::stoi(node.substr(colonPos + 1)));
        } catch (const std::exception& e) {
            getLogger()->warning("Invalid port in bootstrap node {}: {}", node, e.what());
            continue;
        }

        try {
            // Resolve the hostname
            auto addresses = dht_hunter::network::AddressResolver::resolve(host);
            if (!addresses.empty()) {
                bootstrapEndpoints.emplace_back(addresses[0], bootstrapPort);
                getLogger()->info("Resolved bootstrap node {} to {}", node, bootstrapEndpoints.back().toString());
            } else {
                getLogger()->warning("Failed to resolve bootstrap node: {}", host);
            }
        } catch (const std::exception& e) {
            getLogger()->warning("Exception resolving bootstrap node {}: {}", host, e.what());
        }
    }

    // Bootstrap with the resolved endpoints
    if (!bootstrapEndpoints.empty()) {
        if (!dhtNode->bootstrap(bootstrapEndpoints)) {
            getLogger()->warning("Failed to bootstrap DHT node");
            return false;
        } else {
            getLogger()->info("DHT node bootstrapped successfully");
            return true;
        }
    } else {
        getLogger()->warning("No valid bootstrap nodes found");
        return false;
    }
}

/**
 * @brief Print DHT statistics
 * @param dhtNode The DHT node
 * @param collector The infohash collector
 * @param lastInfoHashCount Reference to the last infohash count
 * @param startTime The start time of the application
 */
void printStatistics(std::shared_ptr<dht_hunter::dht::DHTNode> dhtNode,
                     std::shared_ptr<dht_hunter::crawler::InfoHashCollector> collector,
                     size_t& lastInfoHashCount,
                     const std::chrono::steady_clock::time_point& startTime) {
    // Get routing table statistics
    size_t nodeCount = dhtNode->getRoutingTable().size();
    size_t bucketCount = dhtNode->getRoutingTable().getBuckets().size();

    // Get info hash statistics
    size_t infoHashCount = collector->getInfoHashCount();
    size_t newInfoHashes = infoHashCount - lastInfoHashCount;
    lastInfoHashCount = infoHashCount;

    // Calculate uptime
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
    auto days = static_cast<int>(uptime / 86400);
    auto hours = static_cast<int>((uptime % 86400) / 3600);
    auto minutes = static_cast<int>((uptime % 3600) / 60);
    auto seconds = static_cast<int>(uptime % 60);

    // Calculate infohash rate
    double infoHashRate = static_cast<double>(infoHashCount) / (uptime > 0 ? uptime : 1);

    // Log statistics
    getLogger()->info("DHT Statistics:");
    getLogger()->info("  Uptime: {}d {:02d}h {:02d}m {:02d}s", days, hours, minutes, seconds);
    getLogger()->info("  Nodes: {} in {} buckets", nodeCount, bucketCount);
    getLogger()->info("  InfoHashes: {} total (+{} new)", infoHashCount, newInfoHashes);
    getLogger()->info("  InfoHash Rate: {:.2f} per second", infoHashRate);
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    CommandLineArgs args = parseCommandLine(argc, argv);

    // Log the actual log filename being used
    getLogger()->info("Using log file: {}.log", dht_hunter::util::FilesystemUtils::getExecutableName());

    // Ensure config directory exists
    if (!dht_hunter::util::FilesystemUtils::ensureDirectoryExists(args.configDir)) {
        getLogger()->error("Failed to create config directory: {}", args.configDir);
        return 1;
    }

    // Register signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    getLogger()->info("Starting DHT node on port {} with config directory '{}'", args.port, args.configDir);
    getLogger()->info("Maximum infohashes: {}, Save interval: {} seconds",
                 args.maxInfoHashes, args.saveInterval.count());

    // Record start time for uptime calculation
    auto startTime = std::chrono::steady_clock::now();

    // Create and start the DHT node
    g_dhtNode = std::make_shared<dht_hunter::dht::DHTNode>(args.port, args.configDir);

    // Create an InfoHash collector
    g_collector = std::make_shared<dht_hunter::crawler::InfoHashCollector>();
    g_dhtNode->setInfoHashCollector(g_collector);

    // Configure the InfoHash collector
    configureInfoHashCollector(g_collector, args, args.configDir);

    // Start the DHT node
    if (!g_dhtNode->start()) {
        getLogger()->error("Failed to start DHT node");
        return 1;
    }

    // Bootstrap the DHT node
    bootstrapDHTNode(g_dhtNode);

    // Main loop
    getLogger()->info("DHT node running, press Ctrl+C to exit");

    // Statistics variables
    size_t lastInfoHashCount = 0;
    auto lastStatsTime = std::chrono::steady_clock::now();
    auto lastLookupTime = std::chrono::steady_clock::now();
    auto lastSaveTime = std::chrono::steady_clock::now();

    while (g_running) {
        // Sleep for a short time
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Check if shutdown was requested
        if (g_shutdown_requested.load()) {
            break;
        }

        // Current time
        auto now = std::chrono::steady_clock::now();

        // Print statistics every minute
        if (now - lastStatsTime >= std::chrono::minutes(1)) {
            printStatistics(g_dhtNode, g_collector, lastInfoHashCount, startTime);
            lastStatsTime = now;
        }

        // Perform a random lookup every 5 minutes to keep the routing table fresh
        if (now - lastLookupTime >= std::chrono::minutes(5)) {
            dht_hunter::dht::NodeID randomID = dht_hunter::dht::generateRandomNodeID();
            g_dhtNode->findClosestNodes(randomID, [](const std::vector<std::shared_ptr<dht_hunter::dht::Node>>& nodes) {
                getLogger()->debug("Random lookup completed, found {} nodes", nodes.size());
            });
            lastLookupTime = now;
        }

        // Save routing table every 15 minutes
        if (now - lastSaveTime >= std::chrono::minutes(15)) {
            getLogger()->info("Periodic saving of routing table");
            if (g_dhtNode->saveRoutingTable(args.configDir + "/routing_table.dat")) {
                getLogger()->info("Routing table saved successfully");
            }
            lastSaveTime = now;
        }
    }

    // Graceful shutdown
    getLogger()->info("Initiating graceful shutdown");

    // Stop the InfoHash collector first
    getLogger()->info("Stopping InfoHash collector");
    g_collector->stop();

    // Save infohashes before stopping
    std::string infoHashPath = args.configDir + "/infohashes.dat";
    getLogger()->info("Saving infohashes to {}", infoHashPath);
    if (g_collector->saveInfoHashes(infoHashPath)) {
        getLogger()->info("Infohashes saved successfully");
    } else {
        getLogger()->error("Failed to save infohashes");
    }

    // Stop the DHT node
    getLogger()->info("Stopping DHT node");
    g_dhtNode->stop();

    // Save the routing table
    getLogger()->info("Saving routing table");
    if (g_dhtNode->saveRoutingTable(args.configDir + "/routing_table.dat")) {
        getLogger()->info("Routing table saved successfully");
    }

    // Print final statistics
    size_t finalInfoHashCount = g_collector->getInfoHashCount();
    getLogger()->info("Final statistics: {} infohashes collected", finalInfoHashCount);

    // Release resources
    g_dhtNode.reset();
    g_collector.reset();

    // Flush logs before exit
    dht_hunter::logforge::LogForge::flush();

    getLogger()->info("DHT Hunter stopped successfully");
    return 0;
}