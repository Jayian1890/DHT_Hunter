#pragma once

#include <string>
#include <filesystem>
#include <functional>
#include <optional>
#include <iostream>

namespace dht_hunter::utility::filesystem {

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

    /**
     * Gets the name of the current executable without extension
     *
     * @return The executable name without extension, or empty string if it cannot be determined
     */
    static std::string getExecutableName();

    /**
     * Gets the full path to the current executable
     *
     * @return The full path to the executable, or empty optional if it cannot be determined
     */
    static std::optional<std::filesystem::path> getExecutablePath();

    /**
     * Sets the terminal window title using ANSI escape sequences
     *
     * @param title The title to set
     */
    static void setTerminalTitle(const std::string& title);
};

} // namespace dht_hunter::utility::filesystem
