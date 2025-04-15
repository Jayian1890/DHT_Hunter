#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include "include/dht_hunter/logforge/logforge.hpp"
#include "include/dht_hunter/logforge/rotating_file_sink.hpp"

namespace lf = dht_hunter::logforge;
namespace fs = std::filesystem;

int main() {
    std::cout << "Testing LogForge with log rotation" << std::endl;
    
    // Create a console sink
    auto consoleSink = std::make_shared<lf::ConsoleSink>(true);
    consoleSink->setLevel(lf::LogLevel::INFO);
    
    // Create a rotating file sink with size-based rotation
    auto fileSink = std::make_shared<lf::RotatingFileSink>(
        "test_rotation.log",
        1024 * 10,  // 10 KB max size
        3           // Keep 3 files
    );
    fileSink->setLevel(lf::LogLevel::INFO);
    
    // Add sinks to LogForge
    lf::LogForge::addSink(consoleSink);
    lf::LogForge::addSink(fileSink);
    
    // Get a logger
    auto logger = lf::LogForge::getLogger("RotationTest");
    
    // Log some messages to trigger rotation
    logger->info("Starting log rotation test");
    
    // Generate a bunch of log messages to trigger rotation
    for (int i = 0; i < 1000; ++i) {
        std::string padding(100, 'X');
        logger->info("Log message #{} with padding: {}", i, padding);
        
        if (i % 100 == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    logger->info("Log rotation test completed");
    
    // List log files
    std::cout << "\nLog files in directory:" << std::endl;
    for (const auto& entry : fs::directory_iterator(".")) {
        const auto& path = entry.path();
        if (path.stem().string().find("test_rotation") == 0 && 
            path.extension() == ".log") {
            std::cout << path.filename().string() << " - " 
                      << fs::file_size(path) << " bytes" << std::endl;
        }
    }
    
    return 0;
}
