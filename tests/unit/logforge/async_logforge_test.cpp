#include <gtest/gtest.h>
#include "dht_hunter/logforge/logforge.hpp"
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;
namespace lf = dht_hunter::logforge;

class AsyncLogForgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a clean test environment
        if (fs::exists("async_test.log")) {
            fs::remove("async_test.log");
        }
    }

    void TearDown() override {
        // Clean up
        if (fs::exists("async_test.log")) {
            fs::remove("async_test.log");
        }
    }
};

// Test asynchronous logger initialization
TEST_F(AsyncLogForgeTest, Initialization) {
    // Initialize the logger with async enabled
    lf::LogForge::init(
        lf::LogLevel::INFO,
        lf::LogLevel::DEBUG,
        "async_test.log",
        true,  // Use colors
        true   // Enable async
    );
    
    // Verify async logging is enabled
    ASSERT_TRUE(lf::LogForge::isAsyncLoggingEnabled());
}

// Test basic asynchronous logging
TEST_F(AsyncLogForgeTest, BasicLogging) {
    // Initialize the logger with async enabled
    lf::LogForge::init(
        lf::LogLevel::INFO,
        lf::LogLevel::DEBUG,
        "async_test.log",
        true,  // Use colors
        true   // Enable async
    );
    
    // Get a logger instance
    auto logger = lf::LogForge::getLogger("async_test");
    
    // Log some messages
    logger->debug("This is a debug message");
    logger->info("This is an info message");
    logger->warning("This is a warning message");
    logger->error("This is an error message");
    
    // Flush to ensure all messages are written
    lf::LogForge::flush();
    
    // Verify the log file exists
    ASSERT_TRUE(fs::exists("async_test.log"));
    
    // Read the log file
    std::ifstream file("async_test.log");
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Verify the log messages were written
    EXPECT_TRUE(content.find("This is a debug message") != std::string::npos);
    EXPECT_TRUE(content.find("This is an info message") != std::string::npos);
    EXPECT_TRUE(content.find("This is a warning message") != std::string::npos);
    EXPECT_TRUE(content.find("This is an error message") != std::string::npos);
}

// Test high volume logging
TEST_F(AsyncLogForgeTest, HighVolumeLogging) {
    // Initialize the logger with async enabled
    lf::LogForge::init(
        lf::LogLevel::INFO,
        lf::LogLevel::DEBUG,
        "async_test.log",
        true,  // Use colors
        true   // Enable async
    );
    
    // Get a logger instance
    auto logger = lf::LogForge::getLogger("async_test");
    
    // Log a large number of messages
    const int numMessages = 1000;
    for (int i = 0; i < numMessages; ++i) {
        logger->info("High volume test message {}", i);
    }
    
    // Flush to ensure all messages are written
    lf::LogForge::flush();
    
    // Verify the log file exists
    ASSERT_TRUE(fs::exists("async_test.log"));
    
    // Read the log file
    std::ifstream file("async_test.log");
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Verify some sample messages were written
    EXPECT_TRUE(content.find("High volume test message 0") != std::string::npos);
    EXPECT_TRUE(content.find("High volume test message 500") != std::string::npos);
    EXPECT_TRUE(content.find("High volume test message 999") != std::string::npos);
}

// Test performance comparison between sync and async logging
TEST_F(AsyncLogForgeTest, PerformanceComparison) {
    const int numMessages = 1000;
    
    // Test synchronous logging performance
    {
        // Initialize the logger with async disabled
        lf::LogForge::init(
            lf::LogLevel::INFO,
            lf::LogLevel::DEBUG,
            "async_test.log",
            true,  // Use colors
            false  // Disable async
        );
        
        auto logger = lf::LogForge::getLogger("sync_test");
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numMessages; ++i) {
            logger->info("Sync performance test message {}", i);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto syncDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        std::cout << "Synchronous logging time for " << numMessages << " messages: " 
                  << syncDuration << "ms" << std::endl;
    }
    
    // Clean up between tests
    if (fs::exists("async_test.log")) {
        fs::remove("async_test.log");
    }
    
    // Test asynchronous logging performance
    {
        // Initialize the logger with async enabled
        lf::LogForge::init(
            lf::LogLevel::INFO,
            lf::LogLevel::DEBUG,
            "async_test.log",
            true,  // Use colors
            true   // Enable async
        );
        
        auto logger = lf::LogForge::getLogger("async_test");
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numMessages; ++i) {
            logger->info("Async performance test message {}", i);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto asyncDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        std::cout << "Asynchronous logging time for " << numMessages << " messages: " 
                  << asyncDuration << "ms" << std::endl;
        
        // Flush to ensure all messages are written before the test ends
        lf::LogForge::flush();
    }
    
    // Note: We don't make assertions about the relative performance
    // as it can vary by system, but we expect async to be faster
}
