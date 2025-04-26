#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <iomanip>
#include <sstream>
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/event/event_bus.hpp"
#include "dht_hunter/event/log_event_handler.hpp"
#include "dht_hunter/event/logger.hpp"
#include "dht_hunter/network/udp_server.hpp"
#include "dht_hunter/dht/core/dht_node.hpp"
#include "dht_hunter/dht/core/dht_config.hpp"

// Global variables for signal handling
std::atomic<bool> g_running(true);
std::shared_ptr<dht_hunter::network::UDPServer> g_server;
std::shared_ptr<dht_hunter::dht::DHTNode> g_dhtNode;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down gracefully..." << std::endl;
    g_running = false;

    // Stop the DHT node if it's running
    if (g_dhtNode) {
        g_dhtNode->stop();
    }
}

int main(int /*argc*/, char* /*argv*/[]) {
    // Initialize the LogForge singleton with default settings
    auto& logForge = dht_hunter::logforge::LogForge::getInstance();
    logForge.init(dht_hunter::logforge::LogLevel::TRACE,  // Console level
                 dht_hunter::logforge::LogLevel::TRACE,  // File level
                 "dht_hunter.log",                     // Explicitly specify log filename
                 true,                                   // Use colors
                 false);                                // Async logging disabled

    // Create and register the LogForge event handler
    auto logEventHandler = dht_hunter::event::LogEventHandler::create();
    dht_hunter::event::EventBus::getInstance().subscribe("LogEvent", logEventHandler);

    // Create a logger for the main component
    auto logger = dht_hunter::event::Logger::forComponent("Main");

    // Log messages
    logger.info("Using event-based logging system");

    // Register signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);  // Ctrl+C
    std::signal(SIGTERM, signalHandler); // Termination request

    // Default DHT port
    constexpr uint16_t DHT_PORT = 6881;

    // Create a DHT configuration
    dht_hunter::dht::DHTConfig dhtConfig(DHT_PORT);

    // Create a DHT node
    g_dhtNode = std::make_shared<dht_hunter::dht::DHTNode>(dhtConfig);

    // Start the DHT node
    if (!g_dhtNode->start()) {
        logger.error("Failed to start DHT node on port {}", DHT_PORT);
        return 1;
    }

    logger.info("DHT node started with ID: {}", dht_hunter::dht::nodeIDToString(g_dhtNode->getNodeID()));

    // Wait for the node to bootstrap
    logger.info("Waiting for DHT node to bootstrap...");
    std::this_thread::sleep_for(std::chrono::seconds(5));

    while (g_running) {
        // Sleep to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Clean up
    logger.info("Shutting down DHT node");
    if (g_dhtNode) {
        g_dhtNode->stop();
        g_dhtNode.reset();
    }

    logger.info("DHT node shutdown complete");
    return 0;
}
