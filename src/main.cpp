// Standard library includes
#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <thread>

// Project includes
#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/dht/core/dht_node.hpp"
#include "dht_hunter/event/event_bus.hpp"
#include "dht_hunter/event/log_event_handler.hpp"
#include "dht_hunter/event/logger.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/network/udp_server.hpp"

/**
 * Global variables for signal handling and application state
 */
std::atomic<bool> g_running(true);
std::shared_ptr<dht_hunter::network::UDPServer> g_server;
std::shared_ptr<dht_hunter::dht::DHTNode> g_dhtNode;

/**
 * Signal handler for graceful shutdown
 * @param signal The signal received
 */
void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down gracefully..." << std::endl;
    g_running = false;

    // Stop the DHT node if it's running
    if (g_dhtNode) {
        g_dhtNode->stop();
    }
}

/**
 * Main entry point for the DHT Hunter application
 * @param argc Number of command-line arguments
 * @param argv Array of command-line arguments
 * @return Exit code (0 for success, non-zero for error)
 */
int main(int argc, char *argv[]) {
    std::string configDir;

#ifdef _WIN32
    // Windows
    const char *homeDir = std::getenv("USERPROFILE");
#else
    // Unix/Linux/macOS
    const char *homeDir = std::getenv("HOME");
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

    // Initialize the LogForge singleton with default settings
    auto &logForge = dht_hunter::logforge::LogForge::getInstance();
    std::string logFilePath = (configPath / "dht_hunter.log").string();
    logForge.init(
        dht_hunter::logforge::LogLevel::INFO,  // Console level
        dht_hunter::logforge::LogLevel::TRACE,  // File level
        logFilePath,                           // Log file in config directory
        true,                                  // Use colors
        false                                  // Async logging disabled
    );

    // Create and register the LogForge event handler
    auto logEventHandler = dht_hunter::event::LogEventHandler::create();
    dht_hunter::event::EventBus::getInstance().subscribe("LogEvent", logEventHandler);

    // Create a logger for the main component
    auto logger = dht_hunter::event::Logger::forComponent("Main");
    logger.info("Using event-based logging system");

    // Register signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);   // Ctrl+C
    std::signal(SIGTERM, signalHandler);  // Termination request

    // Default DHT port
    constexpr uint16_t DHT_PORT = 6881;

    // Create a DHT configuration with the specified config directory
    dht_hunter::dht::DHTConfig dhtConfig(DHT_PORT, configDir);

    // Set the routing table path to be in the config directory
    dhtConfig.setRoutingTablePath(dhtConfig.getFullPath("routing_table.dat"));

    // Create and start a DHT node
    g_dhtNode = std::make_shared<dht_hunter::dht::DHTNode>(dhtConfig);
    if (!g_dhtNode->start()) {
        logger.error("Failed to start DHT node on port {}", DHT_PORT);
        return 1;
    }

    // Wait for the node to bootstrap
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Main application loop
    while (g_running) {
        // Sleep to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Clean up resources
    logger.info("Shutting down DHT node");
    if (g_dhtNode) {
        g_dhtNode->stop();
        g_dhtNode.reset();
    }

    logger.info("DHT node shutdown complete");
    return 0;
}
