#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <filesystem>
#include <dht_hunter/logforge/logger_macros.hpp>

#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/log_initializer.hpp"
#include "dht_hunter/crawler/dht_crawler.hpp"
#include "dht_hunter/util/filesystem_utils.hpp"

namespace {
    // This will be initialized during static initialization, before main() is called
    struct LogInitializer {
        LogInitializer() {
            // Initialize logging with empty filename to use executable name
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
    static LogInitializer g_logInitializer;
}

DEFINE_COMPONENT_LOGGER("Main", "Application")

// Global variables for signal handling
std::atomic<bool> g_running(true);
std::atomic<bool> g_shutdown_requested(false);
std::shared_ptr<dht_hunter::crawler::DHTCrawler> g_crawler;

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
 * @brief Create a DHTCrawler with default settings
 * @return Configured DHTCrawler instance
 */
std::shared_ptr<dht_hunter::crawler::DHTCrawler> createCrawler() {
    // Create default crawler config
    dht_hunter::crawler::DHTCrawlerConfig config;

    // Get the executable path and set the config directory next to it
    auto execPath = dht_hunter::util::FilesystemUtils::getExecutablePath();
    std::string configDir = "config"; // Default fallback

    if (execPath) {
        // Set config directory next to the executable
        configDir = (execPath->parent_path() / "config").string();
        getLogger()->info("Setting config directory next to executable: {}", configDir);
    } else {
        getLogger()->warning("Could not determine executable path, using default config directory: {}", configDir);
    }

    // Set default values for crawler config
    config.dhtPort = 6881;
    config.configDir = configDir;
    config.maxConcurrentLookups = 50;  // Increased for better performance
    config.maxLookupsPerMinute = 500;  // Increased for better performance
    config.lookupInterval = std::chrono::milliseconds(50);  // Faster lookups
    config.statusInterval = std::chrono::seconds(60);

    // Set bootstrap nodes
    config.bootstrapNodes = {
        "router.bittorrent.com:6881",
        "dht.transmissionbt.com:6881",
        "router.utorrent.com:6881",
        "router.bitcomet.com:6881",
        "dht.aelitis.com:6881"
    };

    // Set InfoHash collector config
    config.infoHashCollectorConfig.maxStoredInfoHashes = 10000000;  // Store up to 10 million infohashes
    config.infoHashCollectorConfig.saveInterval = std::chrono::minutes(5);  // Save every 5 minutes
    config.infoHashCollectorConfig.deduplicateInfoHashes = true;
    config.infoHashCollectorConfig.filterInvalidInfoHashes = true;
    config.infoHashCollectorConfig.maxQueueSize = 1000000;  // Large queue size
    config.infoHashCollectorConfig.savePath = config.configDir + "/infohashes.dat";

    // Update metadata storage directory to use the new config directory
    config.metadataStorageDirectory = config.configDir + "/metadata";

    // Create the crawler
    auto crawler = std::make_shared<dht_hunter::crawler::DHTCrawler>(config);

    // Set status callback for logging
    crawler->setStatusCallback([](uint64_t infoHashesDiscovered, uint64_t infoHashesQueued,
                                uint64_t metadataFetched, double lookupRate, double metadataFetchRate,
                                uint64_t totalInfoHashes) {
        getLogger()->info("Crawler Status:");
        getLogger()->info("  InfoHashes Discovered: {} (session) / {} (total)", infoHashesDiscovered, totalInfoHashes);
        getLogger()->info("  InfoHashes Queued: {}", infoHashesQueued);
        getLogger()->info("  Metadata Fetched: {}", metadataFetched);
        getLogger()->info("  Lookup Rate: {:.2f} lookups/minute", lookupRate);
        getLogger()->info("  Metadata Fetch Rate: {:.2f} fetches/minute", metadataFetchRate);
    });

    return crawler;
}

int main(int /*argc*/, char* /*argv*/[]) {
    // Log the actual log filename being used
    getLogger()->info("Using log file: {}.log", dht_hunter::util::FilesystemUtils::getExecutableName());

    // Set initial window title
    std::string initialTitle = dht_hunter::util::FilesystemUtils::getExecutableName() + " - Starting...";
    dht_hunter::util::FilesystemUtils::setTerminalTitle(initialTitle);

    // Config directory will be created by the crawler when it starts

    // Register signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Record start time for uptime calculation
    auto startTime = std::chrono::steady_clock::now();

    // Create and start the crawler
    getLogger()->info("Creating DHT Crawler with optimized settings");
    g_crawler = createCrawler();

    // Start the crawler
    getLogger()->info("Starting DHT Crawler...");
    if (!g_crawler->start()) {
        getLogger()->error("Failed to start DHT Crawler");
        return 1;
    }

    getLogger()->info("DHT Crawler started successfully");

    // Main loop
    getLogger()->info("DHT Hunter running, press Ctrl+C to exit");

    // Sleep until shutdown is requested
    while (g_running) {
        // Sleep for a short time
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Check if shutdown was requested
        if (g_shutdown_requested.load()) {
            break;
        }
    }

    // Graceful shutdown
    getLogger()->info("Initiating graceful shutdown");

    // Stop the crawler
    getLogger()->info("Stopping DHT Crawler");
    g_crawler->stop();

    // Get final statistics
    uint64_t infoHashesDiscovered = g_crawler->getInfoHashesDiscovered();
    uint64_t metadataFetched = g_crawler->getMetadataFetched();

    // Get total infohashes
    uint64_t totalInfoHashes = g_crawler->getTotalInfoHashes();

    // Print final statistics
    getLogger()->info("Final statistics:");
    getLogger()->info("  InfoHashes discovered: {} (session) / {} (total)", infoHashesDiscovered, totalInfoHashes);
    getLogger()->info("  Metadata items fetched: {}", metadataFetched);

    // Calculate uptime
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
    auto days = static_cast<int>(uptime / 86400);
    auto hours = static_cast<int>((uptime % 86400) / 3600);
    auto minutes = static_cast<int>((uptime % 3600) / 60);
    auto seconds = static_cast<int>(uptime % 60);

    // Calculate infohash rate
    double infoHashRate = static_cast<double>(infoHashesDiscovered) / (uptime > 0 ? uptime : 1);

    getLogger()->info("  Uptime: {}d {:02d}h {:02d}m {:02d}s", days, hours, minutes, seconds);
    getLogger()->info("  InfoHash discovery rate: {:.2f} per second", infoHashRate);

    // Set final window title with stats
    std::string finalTitle = dht_hunter::util::FilesystemUtils::getExecutableName() +
                           " - Stopped - InfoHashes: " + std::to_string(totalInfoHashes) +
                           " - Metadata: " + std::to_string(metadataFetched);
    dht_hunter::util::FilesystemUtils::setTerminalTitle(finalTitle);

    // Release resources
    g_crawler.reset();

    // Flush logs before exit
    dht_hunter::logforge::LogForge::flush();

    getLogger()->info("DHT Hunter stopped successfully");
    return 0;
}
