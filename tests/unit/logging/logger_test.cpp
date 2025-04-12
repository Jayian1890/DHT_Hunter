#include <gtest/gtest.h>
#include "dht_hunter/logging/logger.hpp"
#include <sstream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a clean test environment
        if (fs::exists("test.log")) {
            fs::remove("test.log");
        }
    }

    void TearDown() override {
        // Clean up
        if (fs::exists("test.log")) {
            fs::remove("test.log");
        }
    }
};

// Test logger initialization
TEST_F(LoggerTest, Initialization) {
    // Initialize the logger
    dht_hunter::logging::Logger::init(
        dht_hunter::logging::LogLevel::INFO,
        dht_hunter::logging::LogLevel::DEBUG,
        "test.log"
    );
    
    // Get a logger instance
    auto logger = dht_hunter::logging::Logger::getLogger("test");
    
    // Verify the logger was created
    ASSERT_NE(logger, nullptr);
}

// Test logging to file
TEST_F(LoggerTest, LogToFile) {
    // Initialize the logger
    dht_hunter::logging::Logger::init(
        dht_hunter::logging::LogLevel::INFO,
        dht_hunter::logging::LogLevel::DEBUG,
        "test.log"
    );
    
    // Get a logger instance
    auto logger = dht_hunter::logging::Logger::getLogger("test");
    
    // Log some messages
    logger->debug("This is a debug message");
    logger->info("This is an info message");
    logger->warning("This is a warning message");
    logger->error("This is an error message");
    
    // Verify the log file exists
    ASSERT_TRUE(fs::exists("test.log"));
    
    // Read the log file
    std::ifstream file("test.log");
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Verify the log messages were written
    EXPECT_TRUE(content.find("This is a debug message") != std::string::npos);
    EXPECT_TRUE(content.find("This is an info message") != std::string::npos);
    EXPECT_TRUE(content.find("This is a warning message") != std::string::npos);
    EXPECT_TRUE(content.find("This is an error message") != std::string::npos);
}

// Test log levels
TEST_F(LoggerTest, LogLevels) {
    // Initialize the logger with INFO level
    dht_hunter::logging::Logger::init(
        dht_hunter::logging::LogLevel::INFO,
        dht_hunter::logging::LogLevel::INFO,
        "test.log"
    );
    
    // Get a logger instance
    auto logger = dht_hunter::logging::Logger::getLogger("test");
    
    // Log some messages
    logger->debug("This debug message should not appear");
    logger->info("This info message should appear");
    
    // Read the log file
    std::ifstream file("test.log");
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Verify only the INFO message was written
    EXPECT_TRUE(content.find("This debug message should not appear") == std::string::npos);
    EXPECT_TRUE(content.find("This info message should appear") != std::string::npos);
}

// Test multiple loggers
TEST_F(LoggerTest, MultipleLoggers) {
    // Initialize the logger
    dht_hunter::logging::Logger::init(
        dht_hunter::logging::LogLevel::INFO,
        dht_hunter::logging::LogLevel::DEBUG,
        "test.log"
    );
    
    // Get multiple logger instances
    auto logger1 = dht_hunter::logging::Logger::getLogger("test1");
    auto logger2 = dht_hunter::logging::Logger::getLogger("test2");
    
    // Log some messages
    logger1->info("Message from logger1");
    logger2->info("Message from logger2");
    
    // Read the log file
    std::ifstream file("test.log");
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Verify both messages were written with correct logger names
    EXPECT_TRUE(content.find("[test1]") != std::string::npos);
    EXPECT_TRUE(content.find("[test2]") != std::string::npos);
    EXPECT_TRUE(content.find("Message from logger1") != std::string::npos);
    EXPECT_TRUE(content.find("Message from logger2") != std::string::npos);
}
