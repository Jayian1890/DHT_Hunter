#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <string>
#include <vector>

#include "log_common.hpp"

namespace dht_hunter::logforge {

/**
 * @enum RotationPolicy
 * @brief Defines the policy for when to rotate log files.
 */
enum class RotationPolicy {
    SIZE,    // Rotate based on file size
    TIME,    // Rotate based on time intervals
    BOTH     // Rotate based on either size or time, whichever comes first
};

/**
 * @class RotatingFileSink
 * @brief A log sink that writes to a file with rotation capabilities.
 * 
 * This sink supports rotating log files based on size or time intervals.
 */
class RotatingFileSink final : public LogSink {
public:
    /**
     * @brief Constructs a RotatingFileSink with size-based rotation.
     * @param baseFilename The base name of the log file.
     * @param maxSizeBytes The maximum size of a log file before rotation (in bytes).
     * @param maxFiles The maximum number of log files to keep.
     */
    RotatingFileSink(const std::string& baseFilename, 
                    size_t maxSizeBytes = 10 * 1024 * 1024,  // 10 MB default
                    size_t maxFiles = 5)
        : m_baseFilename(baseFilename),
          m_maxSizeBytes(maxSizeBytes),
          m_maxFiles(maxFiles),
          m_rotationPolicy(RotationPolicy::SIZE),
          m_currentSize(0) {
        openLogFile();
    }

    /**
     * @brief Constructs a RotatingFileSink with time-based rotation.
     * @param baseFilename The base name of the log file.
     * @param rotationInterval The time interval for rotation (in hours).
     * @param maxFiles The maximum number of log files to keep.
     */
    RotatingFileSink(const std::string& baseFilename, 
                    std::chrono::hours rotationInterval,
                    size_t maxFiles = 5)
        : m_baseFilename(baseFilename),
          m_rotationInterval(rotationInterval),
          m_maxFiles(maxFiles),
          m_rotationPolicy(RotationPolicy::TIME),
          m_currentSize(0),
          m_nextRotationTime(std::chrono::system_clock::now() + rotationInterval) {
        openLogFile();
    }

    /**
     * @brief Constructs a RotatingFileSink with both size and time-based rotation.
     * @param baseFilename The base name of the log file.
     * @param maxSizeBytes The maximum size of a log file before rotation (in bytes).
     * @param rotationInterval The time interval for rotation (in hours).
     * @param maxFiles The maximum number of log files to keep.
     */
    RotatingFileSink(const std::string& baseFilename, 
                    size_t maxSizeBytes,
                    std::chrono::hours rotationInterval,
                    size_t maxFiles = 5)
        : m_baseFilename(baseFilename),
          m_maxSizeBytes(maxSizeBytes),
          m_rotationInterval(rotationInterval),
          m_maxFiles(maxFiles),
          m_rotationPolicy(RotationPolicy::BOTH),
          m_currentSize(0),
          m_nextRotationTime(std::chrono::system_clock::now() + rotationInterval) {
        openLogFile();
    }

    ~RotatingFileSink() override {
        if (m_file.is_open()) {
            m_file.close();
        }
    }

    void write(const LogLevel level, const std::string& loggerName,
              const std::string& message,
              const std::chrono::system_clock::time_point& timestamp) override {
        if (!shouldLog(level) || !m_file.is_open()) {
            return;
        }

        const std::lock_guard lock(m_mutex);

        // Check if we need to rotate based on time
        if ((m_rotationPolicy == RotationPolicy::TIME || m_rotationPolicy == RotationPolicy::BOTH) &&
            timestamp >= m_nextRotationTime) {
            rotate();
            m_nextRotationTime = timestamp + m_rotationInterval;
        }

        // Format timestamp
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()) % 1000;
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &time_t);
#else
        localtime_r(&time_t, &tm);
#endif

        // Format the log message
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.'
            << std::setfill('0') << std::setw(3) << ms.count() << " "
            << "[" << logLevelToString(level) << "] "
            << "[" << loggerName << "] "
            << message << std::endl;

        const std::string formattedMessage = oss.str();
        
        // Write to file
        m_file << formattedMessage;
        m_file.flush();
        
        // Update current size
        m_currentSize += formattedMessage.size();
        
        // Check if we need to rotate based on size
        if ((m_rotationPolicy == RotationPolicy::SIZE || m_rotationPolicy == RotationPolicy::BOTH) &&
            m_currentSize >= m_maxSizeBytes) {
            rotate();
        }
    }

private:
    /**
     * @brief Opens the current log file.
     */
    void openLogFile() {
        // Close the current file if it's open
        if (m_file.is_open()) {
            m_file.close();
        }

        // Open the log file
        m_file.open(m_baseFilename, std::ios::out | std::ios::app);
        if (!m_file.is_open()) {
            throw std::runtime_error("Failed to open log file: " + m_baseFilename);
        }

        // Get the current file size
        m_currentSize = std::filesystem::file_size(m_baseFilename);
    }

    /**
     * @brief Rotates the log file.
     */
    void rotate() {
        // Close the current file
        if (m_file.is_open()) {
            m_file.close();
        }

        // Generate timestamp for the rotated file
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &time_t);
#else
        localtime_r(&time_t, &tm);
#endif

        // Format the timestamp
        std::ostringstream timestamp;
        timestamp << std::put_time(&tm, "%Y%m%d-%H%M%S");

        // Create the rotated file name
        std::filesystem::path basePath(m_baseFilename);
        std::string rotatedFilename = 
            basePath.stem().string() + "." + 
            timestamp.str() + 
            basePath.extension().string();
        
        // Rename the current file to the rotated file name
        std::filesystem::rename(m_baseFilename, rotatedFilename);

        // Open a new log file
        m_file.open(m_baseFilename, std::ios::out);
        if (!m_file.is_open()) {
            throw std::runtime_error("Failed to open log file after rotation: " + m_baseFilename);
        }

        // Reset the current size
        m_currentSize = 0;

        // Enforce the maximum number of files
        enforceMaxFiles();
    }

    /**
     * @brief Enforces the maximum number of log files by deleting the oldest ones.
     */
    void enforceMaxFiles() {
        // Get the directory of the base file
        std::filesystem::path basePath(m_baseFilename);
        std::filesystem::path directory = basePath.parent_path();
        if (directory.empty()) {
            directory = ".";
        }

        // Get the stem of the base file
        std::string stem = basePath.stem().string();

        // Find all rotated log files
        std::vector<std::filesystem::path> logFiles;
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            const auto& path = entry.path();
            if (path.stem().string().find(stem) == 0 && 
                path.stem().string() != stem && 
                path.extension() == basePath.extension()) {
                logFiles.push_back(path);
            }
        }

        // Sort by modification time (oldest first)
        std::sort(logFiles.begin(), logFiles.end(), 
                 [](const std::filesystem::path& a, const std::filesystem::path& b) {
                     return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
                 });

        // Delete oldest files if we have too many
        while (logFiles.size() >= m_maxFiles) {
            std::filesystem::remove(logFiles.front());
            logFiles.erase(logFiles.begin());
        }
    }

    std::string m_baseFilename;
    size_t m_maxSizeBytes = 0;
    std::chrono::hours m_rotationInterval{24}; // Default to 24 hours
    size_t m_maxFiles = 5;
    RotationPolicy m_rotationPolicy;
    std::ofstream m_file;
    size_t m_currentSize;
    std::chrono::system_clock::time_point m_nextRotationTime;
    std::mutex m_mutex;
};

} // namespace dht_hunter::logforge
