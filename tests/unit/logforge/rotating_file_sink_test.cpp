#include <gtest/gtest.h>
#include "dht_hunter/logforge/rotating_file_sink.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;
namespace lf = dht_hunter::logforge;

class RotatingFileSinkTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a clean test environment
        cleanupLogFiles();
    }

    void TearDown() override {
        // Clean up
        cleanupLogFiles();
    }

    void cleanupLogFiles() {
        try {
            fs::remove("rotating_test.log");
            for (const auto& entry : fs::directory_iterator(".")) {
                const auto& path = entry.path();
                if (path.stem().string().find("rotating_test") == 0 && 
                    path.extension() == ".log") {
                    fs::remove(path);
                }
            }
        } catch (const std::exception&) {
            // Ignore errors during cleanup
        }
    }
};

// Test size-based rotation
TEST_F(RotatingFileSinkTest, SizeBasedRotation) {
    // Create a rotating file sink with a small max size
    auto sink = std::make_shared<lf::RotatingFileSink>(
        "rotating_test.log",
        500,  // 500 bytes max size
        3     // Keep 3 files
    );
    
    // Write logs to trigger rotation
    for (int i = 0; i < 10; ++i) {
        std::string message = "Test message #" + std::to_string(i) + " with padding: ";
        message += std::string(50, 'X');
        
        sink->write(lf::LogLevel::INFO, "RotationTest", message, 
                   std::chrono::system_clock::now());
    }
    
    // Count the number of log files
    int fileCount = 0;
    for (const auto& entry : fs::directory_iterator(".")) {
        const auto& path = entry.path();
        if (path.stem().string().find("rotating_test") == 0 && 
            path.extension() == ".log") {
            fileCount++;
        }
    }
    
    // We should have the main log file plus rotated files, but not more than maxFiles
    EXPECT_GT(fileCount, 1);
    EXPECT_LE(fileCount, 3);
}

// Test time-based rotation
TEST_F(RotatingFileSinkTest, TimeBasedRotationSetup) {
    // Create a rotating file sink with time-based rotation
    auto sink = std::make_shared<lf::RotatingFileSink>(
        "rotating_test.log",
        std::chrono::hours(24),  // Rotate every 24 hours
        3                        // Keep 3 files
    );
    
    // Write a log message
    sink->write(lf::LogLevel::INFO, "RotationTest", "Test message for time-based rotation", 
               std::chrono::system_clock::now());
    
    // Verify the log file exists
    EXPECT_TRUE(fs::exists("rotating_test.log"));
    
    // We can't easily test actual time-based rotation in a unit test
    // since it would require waiting for the rotation interval
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
