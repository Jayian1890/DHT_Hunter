#include <iostream>
#include <thread>
#include <chrono>
#include "dht_hunter/logging/logger.hpp"

using namespace dht_hunter::logging;

// Function to demonstrate logging from a different thread
void logFromThread(std::shared_ptr<Logger> logger, int threadId) {
    for (int i = 0; i < 5; ++i) {
        logger->info("Message {} from thread {}", i, threadId);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    std::cout << "DHT-Hunter Logging Demo" << std::endl;
    std::cout << "=======================" << std::endl;

    // Initialize the logging system with color support
    Logger::init(LogLevel::TRACE, LogLevel::TRACE, "logging_demo.log", true);

    // Get a logger for the main component
    auto mainLogger = Logger::getLogger("Main");
    mainLogger->info("Logging system initialized with color support");

    // Demonstrate different log levels
    mainLogger->trace("This is a TRACE message");
    mainLogger->debug("This is a DEBUG message");
    mainLogger->info("This is an INFO message");
    mainLogger->warning("This is a WARNING message");
    mainLogger->error("This is an ERROR message");
    mainLogger->critical("This is a CRITICAL message");

    // Demonstrate multiple loggers
    auto networkLogger = Logger::getLogger("Network");
    auto dhtLogger = Logger::getLogger("DHT");

    networkLogger->info("Network component initialized");
    dhtLogger->info("DHT component initialized");

    // Demonstrate logging with parameters
    mainLogger->info("Starting {} threads", 2);

    // Demonstrate logging from multiple threads
    std::thread t1(logFromThread, networkLogger, 1);
    std::thread t2(logFromThread, dhtLogger, 2);

    t1.join();
    t2.join();

    mainLogger->info("All threads completed");

    // Demonstrate changing log levels
    mainLogger->info("Changing log level to WARNING");
    Logger::setGlobalLevel(LogLevel::WARNING);

    mainLogger->debug("This debug message should not appear");
    mainLogger->info("This info message should not appear");
    mainLogger->warning("This warning message should appear");

    // Demonstrate toggling color support
    mainLogger->warning("Now demonstrating color toggling...");

    // Get the console sink to toggle colors
    auto& sinks = Logger::getSinks();
    for (auto& sink : sinks) {
        // Try to cast to ConsoleSink
        auto consoleSink = std::dynamic_pointer_cast<ConsoleSink>(sink);
        if (consoleSink) {
            // Toggle color support
            bool currentColorState = consoleSink->getUseColors();
            consoleSink->setUseColors(!currentColorState);
            mainLogger->info("Color support is now {}", !currentColorState ? "disabled" : "enabled");

            // Show all log levels with the new color setting
            mainLogger->trace("This is a TRACE message with colors {}", !currentColorState ? "disabled" : "enabled");
            mainLogger->debug("This is a DEBUG message with colors {}", !currentColorState ? "disabled" : "enabled");
            mainLogger->info("This is an INFO message with colors {}", !currentColorState ? "disabled" : "enabled");
            mainLogger->warning("This is a WARNING message with colors {}", !currentColorState ? "disabled" : "enabled");
            mainLogger->error("This is an ERROR message with colors {}", !currentColorState ? "disabled" : "enabled");
            mainLogger->critical("This is a CRITICAL message with colors {}", !currentColorState ? "disabled" : "enabled");

            // Toggle back
            consoleSink->setUseColors(currentColorState);
            mainLogger->info("Color support restored to original setting");
        }
    }

    mainLogger->info("Logging demo completed");

    return 0;
}
