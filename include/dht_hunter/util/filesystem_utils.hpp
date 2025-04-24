#pragma once

#include <string>
#include <filesystem>
#include <functional>

namespace dht_hunter::util {

/**
 * Utility class for filesystem operations
 */
class FilesystemUtils {
public:
    /**
     * Creates a directory if it doesn't exist
     * 
     * @param path The directory path to create
     * @param loggerCallback Optional callback for logging
     * @return true if the directory exists or was created successfully, false otherwise
     */
    static bool ensureDirectoryExists(
        const std::filesystem::path& path,
        const std::function<void(const std::string&, const std::string&)>& loggerCallback = nullptr);
    
    /**
     * Creates all parent directories for a file path if they don't exist
     * 
     * @param filePath The file path whose parent directories should be created
     * @param loggerCallback Optional callback for logging
     * @return true if the parent directories exist or were created successfully, false otherwise
     */
    static bool ensureParentDirectoryExists(
        const std::filesystem::path& filePath,
        const std::function<void(const std::string&, const std::string&)>& loggerCallback = nullptr);
    
    /**
     * Checks if a directory is writable by creating a temporary file
     * 
     * @param path The directory path to check
     * @param loggerCallback Optional callback for logging
     * @return true if the directory is writable, false otherwise
     */
    static bool isDirectoryWritable(
        const std::filesystem::path& path,
        const std::function<void(const std::string&, const std::string&)>& loggerCallback = nullptr);
};

} // namespace dht_hunter::util