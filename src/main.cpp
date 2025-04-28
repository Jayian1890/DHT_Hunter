#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <thread>

// Include thread utilities for the global shutdown flag
#include "dht_hunter/utility/thread/thread_utils.hpp"

// Project includes - Types module
#include "dht_hunter/types/node_id.hpp"
#include "dht_hunter/types/endpoint.hpp"
#include "dht_hunter/types/event_types.hpp"

// Project includes - DHT module
#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/dht/core/dht_node.hpp"

// Project includes - Network module
#include "dht_hunter/network/udp_server.hpp"

// Project includes - Unified Event module
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"  // Adapter for old Logger interface
#include "dht_hunter/unified_event/unified_event.hpp"            // New unified event system

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
    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config-dir" || arg == "-c") {
            if (i + 1 < argc) {
                configDir = argv[++i];
            } else {
                std::cerr << "Error: --config-dir requires a directory path" << std::endl;
                return 1;
            }
        } else if (arg == "--verbose" || arg == "-v") {
            verbose = true;
            std::cout << "Verbose mode enabled - trace logs will be displayed" << std::endl;
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
        loggingProcessor->setMinSeverity(dht_hunter::unified_event::EventSeverity::Trace);
        loggingProcessor->setFileOutput(true, logFilePath);
    }

    // Register signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);   // Ctrl+C
    std::signal(SIGTERM, signalHandler);  // Termination request

    // Default DHT port
    constexpr uint16_t DHT_PORT = 6881;

    // Create a DHT configuration with the specified config directory
    dht_hunter::dht::DHTConfig dhtConfig(DHT_PORT, configDir);

    // Configure the logging processor to show trace logs if verbose mode is enabled
    dht_hunter::unified_event::LoggingProcessorConfig loggingConfig;
    loggingConfig.minSeverity = verbose ? dht_hunter::types::EventSeverity::Trace : dht_hunter::types::EventSeverity::Debug;
    dht_hunter::unified_event::configureLoggingProcessor(loggingConfig);

    // Set the routing table path to be in the config directory
    dhtConfig.setRoutingTablePath(dhtConfig.getFullPath("routing_table.dat"));

    // Create and start a DHT node
    g_dhtNode = std::make_shared<dht_hunter::dht::DHTNode>(dhtConfig);
    if (!g_dhtNode->start()) {
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
    if (g_dhtNode) {
        g_dhtNode->stop();
        g_dhtNode.reset();
    }

    // Shutdown the unified event system
    dht_hunter::unified_event::shutdownEventSystem();

    return 0;
}
