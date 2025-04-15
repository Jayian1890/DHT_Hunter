#include <iostream>
#include <filesystem>
#include "include/dht_hunter/logforge/logforge.hpp"

namespace fs = std::filesystem;

int main() {
    std::cout << "Simple Log Rotation Test" << std::endl;
    std::cout << "=======================" << std::endl;
    
    // Clean up any existing log files
    const std::string logFile = "rotation_test.log";
    try {
        fs::remove(logFile);
        for (const auto& entry : fs::directory_iterator(".")) {
            const auto& path = entry.path();
            if (path.stem().string().find("rotation_test") == 0 && 
                path.extension() == ".log") {
                fs::remove(path);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error cleaning up log files: " << e.what() << std::endl;
    }
    
    // Initialize with size-based rotation
    dht_hunter::logforge::LogForge::initWithSizeRotation(
        dht_hunter::logforge::LogLevel::INFO,
        dht_hunter::logforge::LogLevel::INFO,
        logFile,
        1024,  // 1 KB max size
        3      // Keep 3 files
    );
    
    auto logger = dht_hunter::logforge::LogForge::getLogger("RotationTest");
    
    std::cout << "Generating logs to trigger rotation..." << std::endl;
    
    // Generate logs to trigger rotation
    for (int i = 0; i < 100; ++i) {
        std::string padding(30, 'X');
        logger->info("Log message #{} with padding: {}", i, padding);
        
        // Print progress
        if (i % 10 == 0) {
            std::cout << "Generated " << i << " log messages" << std::endl;
        }
    }
    
    std::cout << "\nLog files in directory:" << std::endl;
    for (const auto& entry : fs::directory_iterator(".")) {
        const auto& path = entry.path();
        if (path.stem().string().find("rotation_test") == 0) {
            std::cout << path.filename().string() << " - " 
                      << fs::file_size(path) << " bytes" << std::endl;
        }
    }
    
    return 0;
}
