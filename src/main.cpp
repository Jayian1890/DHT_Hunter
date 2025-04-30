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
#include "dht_hunter/unified_event/unified_event.hpp"            // New unified event system

// Project includes - Web module
#include "dht_hunter/web/web_server.hpp"

/**
 * Global variables for signal handling and application state
 */
std::atomic<bool> g_running(true);
std::shared_ptr<dht_hunter::network::UDPServer> g_server;
std::shared_ptr<dht_hunter::dht::DHTNode> g_dhtNode;
std::shared_ptr<dht_hunter::web::WebServer> g_webServer;

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
    std::string webRoot = "web";
    uint16_t webPort = 8080;

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

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config-dir" || arg == "-c") {
            if (i + 1 < argc) {
                configDir = argv[++i];
            } else {
                std::cerr << "Error: --config-dir requires a directory path" << std::endl;
                return 1;
            }
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

    // Initialize the unified event system
    dht_hunter::unified_event::initializeEventSystem(true,  // Enable logging
                                                     true,  // Enable component communication
                                                     true,  // Enable statistics
                                                     false  // Synchronous processing
    );

    // Configure the logging processor
    std::string logFilePath = (configPath / "dht_hunter.log").string();
    if (auto loggingProcessor = dht_hunter::unified_event::getLoggingProcessor()) {
        loggingProcessor->setMinSeverity(dht_hunter::unified_event::EventSeverity::Info);
        loggingProcessor->setFileOutput(true, logFilePath);
    }

    // Register signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);   // Ctrl+C
    std::signal(SIGTERM, signalHandler);  // Termination request

    // Default DHT port
    constexpr uint16_t DHT_PORT = 6881;

    // Create a DHT configuration with the specified config directory
    dht_hunter::dht::DHTConfig dhtConfig(DHT_PORT, configDir);

    // Initialize the persistence manager
    auto persistenceManager = dht_hunter::dht::PersistenceManager::getInstance(configDir);

    // Create and start a DHT node
    g_dhtNode = std::make_shared<dht_hunter::dht::DHTNode>(dhtConfig);
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

    g_webServer = std::make_shared<dht_hunter::web::WebServer>(
        webRoot,
        webPort,
        statsService,
        routingManager,
        peerStorage
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

    if (persistenceManager) {
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
