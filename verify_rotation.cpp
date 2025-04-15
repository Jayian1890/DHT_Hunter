#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include "include/dht_hunter/logforge/rotating_file_sink.hpp"

namespace fs = std::filesystem;
namespace lf = dht_hunter::logforge;

int main() {
    std::cout << "Verifying Log Rotation Implementation" << std::endl;
    std::cout << "====================================" << std::endl;
    
    const std::string logFile = "verify_rotation.log";
    
    // Clean up any existing log files
    try {
        fs::remove(logFile);
        for (const auto& entry : fs::directory_iterator(".")) {
            const auto& path = entry.path();
            if (path.stem().string().find("verify_rotation") == 0 && 
                path.extension() == ".log") {
                fs::remove(path);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error cleaning up log files: " << e.what() << std::endl;
    }
    
    // Create a rotating file sink with a small max size
    auto sink = std::make_shared<lf::RotatingFileSink>(logFile, 500, 3);
    sink->setLevel(lf::LogLevel::INFO);
    
    std::cout << "Writing logs to trigger rotation..." << std::endl;
    
    // Write logs directly to the sink to trigger rotation
    for (int i = 0; i < 20; ++i) {
        std::string message = "Log message #" + std::to_string(i) + " with padding: ";
        message += std::string(30, 'X');
        
        sink->write(lf::LogLevel::INFO, "RotationTest", message, 
                   std::chrono::system_clock::now());
        
        std::cout << "Wrote log message " << i << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "\nLog files in directory:" << std::endl;
    for (const auto& entry : fs::directory_iterator(".")) {
        const auto& path = entry.path();
        if (path.stem().string().find("verify_rotation") == 0) {
            std::cout << path.filename().string() << " - " 
                      << fs::file_size(path) << " bytes" << std::endl;
        }
    }
    
    return 0;
}
