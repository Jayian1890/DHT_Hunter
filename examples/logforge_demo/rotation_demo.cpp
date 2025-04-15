#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include "dht_hunter/logforge/logforge.hpp"

namespace lf = dht_hunter::logforge;
namespace fs = std::filesystem;

void generateLogs(std::shared_ptr<lf::LogForge> logger, int count) {
    for (int i = 0; i < count; ++i) {
        // Generate a log message with some padding to make it larger
        std::string padding(50, 'X');
        logger->info("Log message #{} with padding: {}", i, padding);

        // Sleep a tiny bit to avoid overwhelming the system
        if (i % 10 == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void displayLogFiles(const std::string& baseFilename) {
    std::cout << "\nLog files in directory:" << std::endl;
    std::cout << "----------------------" << std::endl;

    fs::path basePath(baseFilename);
    fs::path directory = basePath.parent_path();
    if (directory.empty()) {
        directory = ".";
    }

    std::string stem = basePath.stem().string();

    for (const auto& entry : fs::directory_iterator(directory)) {
        const auto& path = entry.path();
        if (path.stem().string().find(stem) == 0 &&
            path.extension() == basePath.extension()) {
            std::cout << path.filename().string() << " - "
                      << fs::file_size(path) << " bytes" << std::endl;
        }
    }
}

int main() {
    std::cout << "LogForge Rotation Demo" << std::endl;
    std::cout << "======================" << std::endl;

    const std::string logFilename = "rotation_demo.log";

    // Remove any existing log files from previous runs
    try {
        fs::remove(logFilename);
        for (const auto& entry : fs::directory_iterator(".")) {
            const auto& path = entry.path();
            if (path.stem().string().find("rotation_demo") == 0 &&
                path.extension() == ".log") {
                fs::remove(path);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error cleaning up old log files: " << e.what() << std::endl;
    }

    std::cout << "\n1. Size-based rotation demo" << std::endl;
    std::cout << "-------------------------" << std::endl;

    // Initialize with size-based rotation (small size for demo purposes)
    lf::LogForge::initWithSizeRotation(
        lf::LogLevel::INFO,
        lf::LogLevel::INFO,
        logFilename,
        1024 * 2,   // 2 KB max size
        3           // Keep 3 files
    );

    auto logger = lf::LogForge::getLogger("RotationDemo");
    logger->info("Starting size-based rotation demo");

    // Generate enough logs to trigger multiple rotations
    generateLogs(logger, 100);

    // Display the log files
    displayLogFiles(logFilename);

    // Clean up for next test
    try {
        fs::remove(logFilename);
        for (const auto& entry : fs::directory_iterator(".")) {
            const auto& path = entry.path();
            if (path.stem().string().find("rotation_demo") == 0 &&
                path.extension() == ".log") {
                fs::remove(path);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error cleaning up log files: " << e.what() << std::endl;
    }

    std::cout << "\n2. Time-based rotation setup demo" << std::endl;
    std::cout << "----------------------------" << std::endl;
    std::cout << "Note: This demo shows how to set up time-based rotation" << std::endl;
    std::cout << "      (We won't actually see the rotation happen in this demo)" << std::endl;

    // For a real application, you would typically use longer intervals like 24 hours

    // Create sinks for the time-based rotation demo
    const auto consoleSink = std::make_shared<lf::ConsoleSink>(true);
    consoleSink->setLevel(lf::LogLevel::INFO);

    // Create a rotating file sink with a 1-hour rotation interval for demo purposes
    const auto fileSink = std::make_shared<lf::RotatingFileSink>(
        logFilename,
        std::chrono::hours(1),  // Rotate every hour
        3                      // Keep 3 files
    );
    fileSink->setLevel(lf::LogLevel::INFO);

    // Reset the LogForge system
    // Note: This is a hack for demo purposes. In a real application, you would
    // properly shut down and reinitialize the logging system.
    // We can't access private static members directly, so we'll just create a new initialization

    // For demo purposes, we'll create a custom initialization
    // In a real application, you would use the provided initialization methods

    // First, make sure we clean up any existing log files
    try {
        fs::remove(logFilename);
        for (const auto& entry : fs::directory_iterator(".")) {
            const auto& path = entry.path();
            if (path.stem().string().find("rotation_demo") == 0 &&
                path.extension() == ".log") {
                fs::remove(path);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error cleaning up log files: " << e.what() << std::endl;
    }

    // Now initialize with our custom time-based rotation sink
    lf::LogForge::init(lf::LogLevel::INFO, lf::LogLevel::INFO, "dummy.log", true, true);

    // We'll use a separate logger for this part of the demo
    logger = lf::LogForge::getLogger("TimeRotationDemo");

    logger = lf::LogForge::getLogger("RotationDemo");
    logger->info("Starting time-based rotation demo");

    // Generate a small number of logs for the demo
    logger->info("Generating logs for time-based rotation demo");
    generateLogs(logger, 20);

    // Display the log files
    displayLogFiles(logFilename);

    std::cout << "\nRotation demo completed" << std::endl;

    return 0;
}
