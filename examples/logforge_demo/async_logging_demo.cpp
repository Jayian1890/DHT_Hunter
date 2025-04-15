#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include "dht_hunter/logforge/logforge.hpp"

namespace lf = dht_hunter::logforge;

// Function to log messages from a thread
void logFromThread(std::shared_ptr<lf::LogForge> logger, int threadId, int numMessages) {
    for (int i = 0; i < numMessages; ++i) {
        logger->info("Thread {} - Message {}", threadId, i);
        
        // Simulate some work
        if (i % 10 == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

int main() {
    std::cout << "LogForge Asynchronous Logging Demo" << std::endl;
    std::cout << "==================================" << std::endl;
    
    // Number of messages and threads for the test
    const int numThreads = 4;
    const int messagesPerThread = 1000;
    const int totalMessages = numThreads * messagesPerThread;
    
    std::cout << "Testing with " << numThreads << " threads, " 
              << messagesPerThread << " messages per thread (" 
              << totalMessages << " total messages)" << std::endl;
    
    // First, test with synchronous logging
    {
        std::cout << "\nSynchronous Logging Test:" << std::endl;
        std::cout << "------------------------" << std::endl;
        
        // Initialize the logger with synchronous logging
        lf::LogForge::init(lf::LogLevel::INFO, lf::LogLevel::DEBUG, 
                          "sync_logging_demo.log", true, false);
        
        auto logger = lf::LogForge::getLogger("SyncDemo");
        logger->info("Starting synchronous logging test");
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Create and start threads
        std::vector<std::thread> threads;
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back(logFromThread, logger, i, messagesPerThread);
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
        
        logger->info("Synchronous logging test completed");
        
        std::cout << "Synchronous logging completed in " << duration 
                  << " ms (" << (totalMessages * 1000.0 / duration) 
                  << " messages/second)" << std::endl;
    }
    
    // Now, test with asynchronous logging
    {
        std::cout << "\nAsynchronous Logging Test:" << std::endl;
        std::cout << "--------------------------" << std::endl;
        
        // Initialize the logger with asynchronous logging
        lf::LogForge::init(lf::LogLevel::INFO, lf::LogLevel::DEBUG, 
                          "async_logging_demo.log", true, true);
        
        auto logger = lf::LogForge::getLogger("AsyncDemo");
        logger->info("Starting asynchronous logging test");
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Create and start threads
        std::vector<std::thread> threads;
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back(logFromThread, logger, i, messagesPerThread);
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
        
        logger->info("Asynchronous logging test completed");
        
        std::cout << "Asynchronous logging completed in " << duration 
                  << " ms (" << (totalMessages * 1000.0 / duration) 
                  << " messages/second)" << std::endl;
        
        // Flush to ensure all messages are written
        std::cout << "Flushing asynchronous log queue..." << std::endl;
        auto flushStart = std::chrono::high_resolution_clock::now();
        
        lf::LogForge::flush();
        
        auto flushEnd = std::chrono::high_resolution_clock::now();
        auto flushDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
            flushEnd - flushStart).count();
        
        std::cout << "Flush completed in " << flushDuration << " ms" << std::endl;
    }
    
    std::cout << "\nLogging demo completed. Check sync_logging_demo.log and async_logging_demo.log for results." << std::endl;
    
    return 0;
}
