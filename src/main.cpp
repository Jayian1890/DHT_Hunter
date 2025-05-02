#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <thread>
#include <sstream>

// Include thread utilities for the global shutdown flag
#include "dht_hunter/utility/thread/thread_utils.hpp"

// Project includes - Types module
#include "dht_hunter/types/event_types.hpp"

// Project includes - DHT module
#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/dht/core/dht_node.hpp"
#include "dht_hunter/dht/services/statistics_service.hpp"
#include "dht_hunter/dht/routing/routing_manager.hpp"
#include "dht_hunter/dht/core/persistence_manager.hpp"

// Project includes - Network module
#include "dht_hunter/network/udp_server.hpp"

// Project includes - Unified Event module
#include "dht_hunter/unified_event/unified_event.hpp"

// Project includes - Web module
#include "dht_hunter/web/web_server.hpp"

// Project includes - BitTorrent module
#include "dht_hunter/bittorrent/metadata/metadata_acquisition_manager.hpp"

// Project includes - Configuration module
#include "dht_hunter/utility/config/configuration_manager.hpp"

/**
 * Global variables for signal handling and application state
 */
std::atomic<bool> g_running(true);
std::shared_ptr<dht_hunter::network::UDPServer> g_server;
std::shared_ptr<dht_hunter::dht::DHTNode> g_dhtNode;
std::shared_ptr<dht_hunter::web::WebServer> g_webServer;
std::shared_ptr<dht_hunter::bittorrent::metadata::MetadataAcquisitionManager> g_metadataManager;

/**
 * Signal handler for graceful shutdown
 * @param signal The signal received
 */
void signalHandler(const int signal) {
    std::cout << "Received signal " << signal << ", shutting down gracefully..." << std::endl;

    // Set the global shutdown flag to prevent new lock acquisitions
    dht_hunter::utility::thread::g_shuttingDown.store(true, std::memory_order_release);

    g_running = false;
}

/**
 * Main entry point for the DHT Hunter application
 * @param argc Number of command-line arguments
 * @param argv Array of command-line arguments
 * @return Exit code (0 for success, non-zero for error)
 */
int main(int argc, char* argv[]) {
    std::string configDir;
    std::string configFile;
    std::string webRoot = "web";
    uint16_t webPort = 8080;
    bool generateDefaultConfig = false;

#ifdef _WIN32
    // Windows
    const char* homeDir = std::getenv("USERPROFILE");
#else
    // Unix/Linux/macOS
    const char* homeDir = std::getenv("HOME");
#endif

    if (homeDir) {
        configDir = std::string(homeDir) + "/dht-hunter";
    } else {
        configDir = "config";
    }

    // Default config file path
    configFile = configDir + "/dht_hunter.json";

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config-dir" || arg == "-c") {
            if (i + 1 < argc) {
                configDir = argv[++i];
                // Update config file path if it wasn't explicitly set
                if (configFile == configDir + "/dht_hunter.json") {
                    configFile = configDir + "/dht_hunter.json";
                }
            } else {
                std::cerr << "Error: --config-dir requires a directory path" << std::endl;
                return 1;
            }
        } else if (arg == "--config-file" || arg == "-f") {
            if (i + 1 < argc) {
                configFile = argv[++i];
            } else {
                std::cerr << "Error: --config-file requires a file path" << std::endl;
                return 1;
            }
        } else if (arg == "--generate-config" || arg == "-g") {
            generateDefaultConfig = true;
        } else if (arg == "--web-root" || arg == "-w") {
            if (i + 1 < argc) {
                webRoot = argv[++i];
            } else {
                std::cerr << "Error: --web-root requires a directory path" << std::endl;
                return 1;
            }
        } else if (arg == "--web-port" || arg == "-p") {
            if (i + 1 < argc) {
                try {
                    webPort = static_cast<uint16_t>(std::stoi(argv[++i]));
                } catch (const std::exception& e) {
                    std::cerr << "Error: --web-port requires a valid port number" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --web-port requires a port number" << std::endl;
                return 1;
            }
        }
    }

    // Ensure the config directory exists
    std::filesystem::path configPath(configDir);
    if (!std::filesystem::exists(configPath)) {
        std::cout << "Creating configuration directory: " << configDir << std::endl;
        if (!std::filesystem::create_directories(configPath)) {
            std::cerr << "Error: Failed to create configuration directory: " << configDir << std::endl;
            return 1;
        }
    }

    // Generate default configuration if requested
    if (generateDefaultConfig) {
        dht_hunter::unified_event::logInfo("Main", "Generating default configuration file at " + configFile);
        if (!dht_hunter::utility::config::ConfigurationManager::generateDefaultConfiguration(configFile)) {
            dht_hunter::unified_event::logError("Main", "Failed to generate default configuration file");
            return 1;
        }
        dht_hunter::unified_event::logInfo("Main", "Default configuration generated successfully");
        if (argc <= 2) { // If only the generate config option was provided, exit
            return 0;
        }
    }

    // Initialize the configuration manager
    auto configManager = dht_hunter::utility::config::ConfigurationManager::getInstance(configFile);
    if (!configManager) {
        std::cerr << "Error: Failed to initialize configuration manager" << std::endl;
        return 1;
    }

    // If the config file exists, load settings from it
    if (std::filesystem::exists(configFile)) {
        // Override command-line arguments with config file values if they exist
        if (configManager->hasKey("web.webRoot")) {
            webRoot = configManager->getString("web.webRoot", webRoot);
        }

        if (configManager->hasKey("web.port")) {
            webPort = static_cast<uint16_t>(configManager->getInt("web.port", webPort));
        }

        if (configManager->hasKey("general.configDir")) {
            std::string configDirFromFile = configManager->getString("general.configDir", configDir);
            // Expand ~ to home directory if present
            if (!configDirFromFile.empty() && configDirFromFile[0] == '~') {
                if (homeDir) {
                    configDirFromFile = std::string(homeDir) + configDirFromFile.substr(1);
                }
            }
            configDir = configDirFromFile;
        }
    } else {
        std::cout << "No configuration file found at: " << configFile << ". Generating default configuration file." << std::endl;

        // Generate default configuration file
        if (!dht_hunter::utility::config::ConfigurationManager::generateDefaultConfiguration(configFile)) {
            std::cerr << "Error: Failed to generate default configuration file" << std::endl;
            return 1;
        }
        std::cout << "Default configuration file generated successfully at: " << configFile << std::endl;

        // Reload the configuration manager with the new file
        configManager = dht_hunter::utility::config::ConfigurationManager::getInstance(configFile);
        if (!configManager) {
            std::cerr << "Error: Failed to initialize configuration manager with new config file" << std::endl;
            return 1;
        }
    }

    // Initialize the unified event system with settings from config
    bool enableLogging = configManager->getBool("event.enableLogging", true);
    bool enableComponent = configManager->getBool("event.enableComponent", true);
    bool enableStatistics = configManager->getBool("event.enableStatistics", true);
    bool asyncProcessing = configManager->getBool("event.asyncProcessing", false);

    dht_hunter::unified_event::initializeEventSystem(
        enableLogging,
        enableComponent,
        enableStatistics,
        asyncProcessing
    );

    // Configure the logging processor
    std::string logFileName = configManager->getString("general.logFile", "dht_hunter.log");
    std::string logFilePath = (configPath / logFileName).string();
    std::string logLevelStr = configManager->getString("general.logLevel", "info");

    dht_hunter::unified_event::EventSeverity logLevel = dht_hunter::unified_event::EventSeverity::Info;
    if (logLevelStr == "trace") {
        logLevel = dht_hunter::unified_event::EventSeverity::Trace;
    } else if (logLevelStr == "debug") {
        logLevel = dht_hunter::unified_event::EventSeverity::Debug;
    } else if (logLevelStr == "info") {
        logLevel = dht_hunter::unified_event::EventSeverity::Info;
    } else if (logLevelStr == "warning") {
        logLevel = dht_hunter::unified_event::EventSeverity::Warning;
    } else if (logLevelStr == "error") {
        logLevel = dht_hunter::unified_event::EventSeverity::Error;
    } else if (logLevelStr == "critical") {
        logLevel = dht_hunter::unified_event::EventSeverity::Critical;
    }

    if (auto loggingProcessor = dht_hunter::unified_event::getLoggingProcessor()) {
        loggingProcessor->setMinSeverity(logLevel);
        bool fileOutput = configManager->getBool("logging.fileOutput", true);
        loggingProcessor->setFileOutput(fileOutput, logFilePath);
        loggingProcessor->setConsoleOutput(configManager->getBool("logging.consoleOutput", true));
    }

    // Register signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);   // Ctrl+C
    std::signal(SIGTERM, signalHandler);  // Termination request

    // Get DHT port from config
    uint16_t dhtPort = static_cast<uint16_t>(configManager->getInt("dht.port", 6881));

    // Create a DHT configuration with settings from config
    dht_hunter::dht::DHTConfig dhtConfig(dhtPort, configDir);

    // Apply additional DHT configuration from config file
    dhtConfig.setKBucketSize(configManager->getInt("dht.kBucketSize", 16));
    dhtConfig.setAlpha(configManager->getInt("dht.alpha", 3));
    dhtConfig.setMaxResults(configManager->getInt("dht.maxResults", 8));
    dhtConfig.setTokenRotationInterval(configManager->getInt("dht.tokenRotationInterval", 300));
    dhtConfig.setBucketRefreshInterval(configManager->getInt("dht.bucketRefreshInterval", 60));
    dhtConfig.setMaxIterations(configManager->getInt("dht.maxIterations", 10));
    dhtConfig.setMaxQueries(configManager->getInt("dht.maxQueries", 100));

    // Set bootstrap nodes from config
    if (configManager->hasKey("dht.bootstrapNodes")) {
        std::vector<std::string> bootstrapNodes = configManager->getStringArray("dht.bootstrapNodes");
        if (!bootstrapNodes.empty()) {
            dhtConfig.setBootstrapNodes(bootstrapNodes);
        }
    }

    // Initialize the persistence manager with settings from config
    auto persistenceManager = dht_hunter::dht::PersistenceManager::getInstance(configDir);

    // Configure persistence manager if available
    if (persistenceManager) {
        // Set save interval from config
        int saveIntervalMinutes = configManager->getInt("persistence.saveInterval", 60);
        persistenceManager->setSaveInterval(std::chrono::minutes(saveIntervalMinutes));
    }

    // Try to load the node ID from disk, or generate a new one if not found
    dht_hunter::dht::NodeID nodeID = dht_hunter::dht::generateRandomNodeID();
    if (persistenceManager) {
        if (persistenceManager->loadNodeID(nodeID)) {
            dht_hunter::unified_event::logInfo("Main", "Loaded existing node ID: " + dht_hunter::dht::nodeIDToString(nodeID));
        } else {
            dht_hunter::unified_event::logInfo("Main", "No existing node ID found, generated new ID: " + dht_hunter::dht::nodeIDToString(nodeID));
            // Save the new node ID
            persistenceManager->saveNodeID(nodeID);
        }
    } else {
        dht_hunter::unified_event::logWarning("Main", "Persistence manager is null, using generated node ID: " + dht_hunter::dht::nodeIDToString(nodeID));
    }

    // Create and start a DHT node with the loaded or generated node ID
    g_dhtNode = std::make_shared<dht_hunter::dht::DHTNode>(dhtConfig, nodeID);
    if (!g_dhtNode->start()) {
        std::cerr << "Failed to start DHT node" << std::endl;
        return 1;
    }

    // Start the persistence manager
    if (persistenceManager) {
        auto routingTable = g_dhtNode->getRoutingTable();
        auto peerStorage = g_dhtNode->getPeerStorage();
        if (!persistenceManager->start(routingTable, peerStorage)) {
            dht_hunter::unified_event::logError("DHT.PersistenceManager", "Failed to start persistence manager");
        }
    } else {
        dht_hunter::unified_event::logCritical("DHT.PersistenceManager", "Persistence manager is null");
    }

    // Wait for the node to bootstrap
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Start the web server
    auto statsService = dht_hunter::dht::services::StatisticsService::getInstance();
    auto routingManager = g_dhtNode->getRoutingManager();
    auto peerStorage = g_dhtNode->getPeerStorage();

    // Start the metadata acquisition manager
    g_metadataManager = dht_hunter::bittorrent::metadata::MetadataAcquisitionManager::getInstance(peerStorage);
    if (!g_metadataManager->start()) {
        dht_hunter::unified_event::logError("Main", "Failed to start metadata acquisition manager");
    } else {
        dht_hunter::unified_event::logInfo("Main", "Metadata acquisition manager started");

        // Trigger metadata acquisition for existing InfoHashes
        if (peerStorage) {
            auto infoHashes = peerStorage->getAllInfoHashes();
            dht_hunter::unified_event::logInfo("Main", "Triggering metadata acquisition for " + std::to_string(infoHashes.size()) + " existing InfoHashes");

            for (const auto& infoHash : infoHashes) {
                g_metadataManager->acquireMetadata(infoHash);
            }
        }
    }

    // Create web server with settings from config
    g_webServer = std::make_shared<dht_hunter::web::WebServer>(
        statsService,
        routingManager,
        peerStorage,
        g_metadataManager
    );

    if (!g_webServer->start()) {
        dht_hunter::unified_event::logError("Main", "Failed to start web server");
    } else {
        dht_hunter::unified_event::logInfo("Main", "Web UI available at http://localhost:" + std::to_string(webPort));
    }

    // Function to update the terminal title with stats
    auto updateTitle = [&]() {
        if (!statsService || !routingManager) {
            return;
        }

        size_t nodesDiscovered = statsService->getNodesDiscovered();
        size_t nodesInTable = routingManager->getNodeCount();
        size_t peersDiscovered = statsService->getPeersDiscovered();
        size_t messagesSent = statsService->getMessagesSent();
        size_t messagesReceived = statsService->getMessagesReceived();

        std::stringstream ss;
        ss << "\033]0;DHT Hunter - Nodes: " << nodesDiscovered
           << "/" << nodesInTable
           << " | Peers: " << peersDiscovered
           << " | Msgs: " << messagesReceived << "/" << messagesSent
           << "\007";

        std::cout << ss.str() << std::flush;
    };

    // Main application loop
    auto lastUpdateTime = std::chrono::steady_clock::now();
    while (g_running) {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastUpdateTime);

        // Update title every second
        if (elapsedTime.count() >= 1000) {
            updateTitle();
            lastUpdateTime = currentTime;
        }

        // Sleep to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Clean up resources
    if (g_webServer) {
        g_webServer->stop();
        g_webServer.reset();
    }

    if (g_metadataManager) {
        g_metadataManager->stop();
        g_metadataManager.reset();
    }

    if (persistenceManager && g_dhtNode) {
        // Save the node ID before stopping
        persistenceManager->saveNodeID(g_dhtNode->getNodeID());
        persistenceManager->stop();
    } else if (persistenceManager) {
        persistenceManager->stop();
    }

    if (g_dhtNode) {
        g_dhtNode->stop();
        g_dhtNode.reset();
    }

    // Shutdown the unified event system
    dht_hunter::unified_event::shutdownEventSystem();

    return 0;
}
