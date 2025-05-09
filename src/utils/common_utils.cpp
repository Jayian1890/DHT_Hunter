/**
 * @file common_utils.cpp
 * @brief Implementation of common utility functions
 */

#include "utils/common_utils.hpp"

// Additional includes for implementation
#include <iomanip>
#include <sstream>

namespace dht_hunter::utility {

//-----------------------------------------------------------------------------
// String utilities implementation
//-----------------------------------------------------------------------------
namespace string {

std::string bytesToHex(const uint8_t* data, size_t length) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (size_t i = 0; i < length; ++i) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }

    return ss.str();
}

std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;

    // Check if the string has an even length
    if (hex.length() % 2 != 0) {
        return bytes;
    }

    // Convert each pair of hex characters to a byte
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteStr = hex.substr(i, 2);
        try {
            bytes.push_back(static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16)));
        } catch (const std::exception&) {
            // Invalid hex string
            bytes.clear();
            return bytes;
        }
    }

    return bytes;
}

std::string formatHex(const uint8_t* data, size_t length, const std::string& separator) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (size_t i = 0; i < length; ++i) {
        if (i > 0 && !separator.empty()) {
            ss << separator;
        }
        ss << std::setw(2) << static_cast<int>(data[i]);
    }

    return ss.str();
}

std::string truncateString(const std::string& str, size_t maxLength) {
    if (str.length() <= maxLength) {
        return str;
    }

    return str.substr(0, maxLength) + "...";
}

} // namespace string

//-----------------------------------------------------------------------------
// Hash utilities implementation
//-----------------------------------------------------------------------------
namespace hash {

// Simple implementation of SHA1 for demonstration purposes
// In a real implementation, we would use a proper cryptographic library
std::vector<uint8_t> sha1(const uint8_t* data, size_t length) {
    // This is a placeholder implementation
    // In a real implementation, we would use a proper SHA1 algorithm
    std::vector<uint8_t> hash(20); // SHA1 hash is 20 bytes

    // Simple hash function for demonstration
    for (size_t i = 0; i < length; ++i) {
        hash[i % 20] ^= data[i];
    }

    return hash;
}

std::vector<uint8_t> sha1(const std::string& data) {
    return sha1(reinterpret_cast<const uint8_t*>(data.data()), data.length());
}

std::vector<uint8_t> sha1(const std::vector<uint8_t>& data) {
    return sha1(data.data(), data.size());
}

std::string sha1Hex(const uint8_t* data, size_t length) {
    std::vector<uint8_t> hash = sha1(data, length);
    return string::bytesToHex(hash.data(), hash.size());
}

std::string sha1Hex(const std::string& data) {
    return sha1Hex(reinterpret_cast<const uint8_t*>(data.data()), data.length());
}

std::string sha1Hex(const std::vector<uint8_t>& data) {
    return sha1Hex(data.data(), data.size());
}

} // namespace hash

//-----------------------------------------------------------------------------
// File utilities implementation
//-----------------------------------------------------------------------------
namespace file {

bool fileExists(const std::string& filePath) {
    return std::filesystem::exists(filePath);
}

bool createDirectory(const std::string& dirPath) {
    if (fileExists(dirPath)) {
        return true;
    }
    return std::filesystem::create_directories(dirPath);
}

size_t getFileSize(const std::string& filePath) {
    if (!fileExists(filePath)) {
        return 0;
    }
    return std::filesystem::file_size(filePath);
}

Result<std::string> readFile(const std::string& filePath) {
    if (!fileExists(filePath)) {
        return Result<std::string>::Error("File does not exist: " + filePath);
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return Result<std::string>::Error("Failed to open file: " + filePath);
    }

    file.seekg(0, std::ios::end);
    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string content(static_cast<size_t>(fileSize), '\0');
    file.read(&content[0], static_cast<std::streamsize>(fileSize));

    if (!file.good()) {
        return Result<std::string>::Error("Failed to read file: " + filePath);
    }

    return Result<std::string>(content);
}

Result<void> writeFile(const std::string& filePath, const std::string& content) {
    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return Result<void>::Error("Failed to open file for writing: " + filePath);
    }

    file.write(content.data(), static_cast<std::streamsize>(content.size()));

    if (!file.good()) {
        return Result<void>::Error("Failed to write to file: " + filePath);
    }

    return Result<void>();
}

} // namespace file

//-----------------------------------------------------------------------------
// Filesystem utilities implementation
//-----------------------------------------------------------------------------
namespace filesystem {

Result<std::vector<std::string>> getFiles(const std::string& dirPath, bool recursive) {
    if (!file::fileExists(dirPath)) {
        return Result<std::vector<std::string>>::Error("Directory does not exist: " + dirPath);
    }

    std::vector<std::string> files;

    try {
        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry.path().string());
                }
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        return Result<std::vector<std::string>>::Error("Failed to list files: " + std::string(e.what()));
    }

    return Result<std::vector<std::string>>(files);
}

Result<std::vector<std::string>> getDirectories(const std::string& dirPath, bool recursive) {
    if (!file::fileExists(dirPath)) {
        return Result<std::vector<std::string>>::Error("Directory does not exist: " + dirPath);
    }

    std::vector<std::string> directories;

    try {
        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
                if (entry.is_directory()) {
                    directories.push_back(entry.path().string());
                }
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
                if (entry.is_directory()) {
                    directories.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        return Result<std::vector<std::string>>::Error("Failed to list directories: " + std::string(e.what()));
    }

    return Result<std::vector<std::string>>(directories);
}

std::string getFileExtension(const std::string& filePath) {
    std::filesystem::path path(filePath);
    return path.extension().string().substr(1); // Remove the leading dot
}

std::string getFileName(const std::string& filePath) {
    std::filesystem::path path(filePath);
    return path.stem().string();
}

std::string getDirectoryPath(const std::string& filePath) {
    std::filesystem::path path(filePath);
    return path.parent_path().string();
}

} // namespace filesystem

} // namespace dht_hunter::utility
