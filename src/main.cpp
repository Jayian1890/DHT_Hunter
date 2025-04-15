#include "dht_hunter/logforge/logforge.hpp"

// Use namespace alias for cleaner code
namespace lf = dht_hunter::logforge;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    // Initialize the logging system with color support
    lf::LogForge::init(lf::LogLevel::TRACE,
                      lf::LogLevel::TRACE,
                      "dht_hunter.log",
                      true);

    const auto logger = lf::LogForge::getLogger("main");

    logger->info("Starting...");

    // TODO: Initialize and run the application

    logger->info("DHT-Hunter shutting down...");
    return 0;
}
