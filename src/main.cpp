#include "dht_hunter/logging/logger.hpp"

// Use namespace alias for cleaner code
namespace dl = dht_hunter::logging;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    // Initialize the logging system with color support
    dl::Logger::init(dl::LogLevel::TRACE,
                    dl::LogLevel::TRACE,
                    "dht_hunter.log",
                    true);

    const auto logger = dl::Logger::getLogger("main");

    logger->info("Starting...");

    // TODO: Initialize and run the application

    logger->info("DHT-Hunter shutting down...");
    return 0;
}
