#pragma once

#include <string>
#include <fstream>
#include <filesystem>

namespace dht_hunter::utility::file {

/**
 * @brief Checks if a file exists
 * @param filePath The file path
 * @return True if the file exists, false otherwise
 */
inline bool fileExists(const std::string& filePath) {
    return std::filesystem::exists(filePath);
}

/**
 * @brief Creates a directory if it doesn't exist
 * @param dirPath The directory path
 * @return True if the directory was created or already exists, false otherwise
 */
inline bool createDirectory(const std::string& dirPath) {
    if (fileExists(dirPath)) {
        return true;
    }
    return std::filesystem::create_directories(dirPath);
}

/**
 * @brief Gets the file size
 * @param filePath The file path
 * @return The file size in bytes, or 0 if the file doesn't exist
 */
inline size_t getFileSize(const std::string& filePath) {
    if (!fileExists(filePath)) {
        return 0;
    }
    return std::filesystem::file_size(filePath);
}

/**
 * @brief Reads a file into a string
 * @param filePath The file path
 * @return The file content as a string, or empty string if the file doesn't exist
 */
inline std::string readFile(const std::string& filePath) {
    if (!fileExists(filePath)) {
        return "";
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    file.seekg(0, std::ios::end);
    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string content(static_cast<size_t>(fileSize), '\0');
    file.read(&content[0], static_cast<std::streamsize>(fileSize));

    return content;
}

/**
 * @brief Writes a string to a file
 * @param filePath The file path
 * @param content The content to write
 * @return True if the file was written successfully, false otherwise
 */
inline bool writeFile(const std::string& filePath, const std::string& content) {
    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    file.write(content.data(), static_cast<std::streamsize>(content.size()));

    return file.good();
}

} // namespace dht_hunter::utility::file
