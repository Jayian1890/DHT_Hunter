#include <iostream>
#include <thread>
#include <chrono>
#include "dht_hunter/logforge/logforge.hpp"

namespace lf = dht_hunter::logforge;

// Function to demonstrate logging from a different thread
void logFromThread(std::shared_ptr<lf::LogForge> logger, int threadId) {
    for (int i = 0; i < 5; ++i) {
        logger->info("Message {} from thread {}", i, threadId);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    std::cout << "LogForge Demo" << std::endl;
    std::cout << "=============" << std::endl;
    
    // Initialize the logging system
    lf::LogForge::init(lf::LogLevel::TRACE, lf::LogLevel::TRACE, "logforge_demo.log");
    
    // Get a logger for the main component
    auto mainLogger = lf::LogForge::getLogger("Main");
    mainLogger->info("LogForge system initialized");
    
    // Demonstrate different log levels
    mainLogger->trace("This is a TRACE message");
    mainLogger->debug("This is a DEBUG message");
    mainLogger->info("This is an INFO message");
    mainLogger->warning("This is a WARNING message");
    mainLogger->error("This is an ERROR message");
    mainLogger->critical("This is a CRITICAL message");
    
    // Demonstrate multiple loggers
    auto networkLogger = lf::LogForge::getLogger("Network");
    auto dhtLogger = lf::LogForge::getLogger("DHT");
    
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
    lf::LogForge::setGlobalLevel(lf::LogLevel::WARNING);
    
    mainLogger->debug("This debug message should not appear");
    mainLogger->info("This info message should not appear");
    mainLogger->warning("This warning message should appear");
    
    mainLogger->info("LogForge demo completed");
    
    return 0;
}
