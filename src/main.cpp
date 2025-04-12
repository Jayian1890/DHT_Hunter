#include <iostream>
#include "dht_hunter/logging/logger.hpp"

int main(int argc, [[maybe_unused]] char* argv[]) {
    std::cout << "DHT-Hunter with Colored Logging" << std::endl;
    std::cout << "==============================" << std::endl;

    // Initialize the logging system with color support
    dht_hunter::logging::Logger::init(dht_hunter::logging::LogLevel::TRACE,
                                     dht_hunter::logging::LogLevel::TRACE,
                                     "dht_hunter.log",
                                     true);

    auto logger = dht_hunter::logging::Logger::getLogger("main");

    logger->info("DHT-Hunter starting up with colored logging...");
    logger->debug("Command line arguments: {}", argc);

    // Demonstrate all log levels with colors
    logger->trace("This is a TRACE message");
    logger->debug("This is a DEBUG message");
    logger->info("This is an INFO message");
    logger->warning("This is a WARNING message");
    logger->error("This is an ERROR message");
    logger->critical("This is a CRITICAL message");

    // TODO: Initialize and run the application

    logger->info("DHT-Hunter shutting down...");
    return 0;
}
