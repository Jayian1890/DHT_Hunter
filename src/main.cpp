#include "dht_hunter/logging/logger.hpp"

int main(int argc, [[maybe_unused]] char* argv[]) {
    // Initialize the logging system with color support
    dht_hunter::logging::Logger::init(dht_hunter::logging::LogLevel::TRACE,
                                     dht_hunter::logging::LogLevel::TRACE,
                                     "dht_hunter.log",
                                     true);

    const auto logger = dht_hunter::logging::Logger::getLogger("main");

    logger->info("Starting...");

    // TODO: Initialize and run the application

    logger->info("DHT-Hunter shutting down...");
    return 0;
}
