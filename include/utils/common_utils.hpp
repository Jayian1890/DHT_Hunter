#pragma once

/**
 * @file common_utils.hpp
 * @brief Common utility functions and types for the BitScrape project
 * 
 * This file consolidates common functionality from various utility files:
 * - common_utils.hpp (Result<T> class and constants)
 * - string_utils.hpp (String manipulation functions)
 * - hash_utils.hpp (Hashing functions)
 * - file_utils.hpp (File I/O operations)
 * - filesystem_utils.hpp (Filesystem operations)
 */

// Standard C++ libraries
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace dht_hunter::utility {

//-----------------------------------------------------------------------------
// Common utilities
//-----------------------------------------------------------------------------
namespace common {

/**
 * @brief Result type for operations that can fail
 *
 * This class provides a way to return either a value or an error message
 * from a function. It is used throughout the codebase for error handling.
 *
 * @tparam T The type of the value to return
 */
template<typename T>
class Result {
public:
    /**
     * @brief Default constructor for internal use
     */
    Result() : m_success(false), m_error("") {}

    /**
     * @brief Construct a successful result with a value
     * @param value The value to return
     */
    Result(const T& value) : m_value(value), m_success(true), m_error("") {}

    /**
     * @brief Construct a successful result with a moved value
     * @param value The value to move
     */
    Result(T&& value) : m_value(std::move(value)), m_success(true), m_error("") {}

    /**
     * @brief Construct a failed result with an error message
     * @param error The error message
     */
    template <typename E>
    Result(E&& error, typename std::enable_if<std::is_convertible<E, std::string>::value>::type* = nullptr)
        : m_success(false), m_error(std::forward<E>(error)) {}

    /**
     * @brief Static method to create an error result
     * @param error The error message
     * @return A Result with an error
     */
    static Result<T> Error(const std::string& error) {
        Result<T> result;
        result.m_success = false;
        result.m_error = error;
        return result;
    }

    /**
     * @brief Check if the result is successful
     * @return true if successful, false otherwise
     */
    bool isSuccess() const { return m_success; }

    /**
     * @brief Check if the result is an error
     * @return true if an error, false otherwise
     */
    bool isError() const { return !m_success; }

    /**
     * @brief Get the error message
     * @return The error message
     */
    const std::string& getError() const { return m_error; }

    /**
     * @brief Get the value (const reference)
     * @return The value
     */
    const T& getValue() const { return m_value; }

    /**
     * @brief Get the value (reference)
     * @return The value
     */
    T& getValue() { return m_value; }

private:
    T m_value;
    bool m_success;
    std::string m_error;
};

/**
 * @brief Specialization of Result for void return type
 */
template<>
class Result<void> {
public:
    /**
     * @brief Construct a successful result
     */
    Result() : m_success(true), m_error("") {}

    /**
     * @brief Construct a failed result with an error message
     * @param error The error message
     */
    Result(const std::string& error) : m_success(false), m_error(error) {}

    /**
     * @brief Static method to create an error result
     * @param error The error message
     * @return A Result with an error
     */
    static Result<void> Error(const std::string& error) {
        Result<void> result;
        result.m_success = false;
        result.m_error = error;
        return result;
    }

    /**
     * @brief Check if the result is successful
     * @return true if successful, false otherwise
     */
    bool isSuccess() const { return m_success; }

    /**
     * @brief Check if the result is an error
     * @return true if an error, false otherwise
     */
    bool isError() const { return !m_success; }

    /**
     * @brief Get the error message
     * @return The error message
     */
    const std::string& getError() const { return m_error; }

private:
    bool m_success;
    std::string m_error;
};

// Common constants
namespace constants {
    constexpr uint16_t DEFAULT_DHT_PORT = 6881;
    constexpr uint16_t DEFAULT_WEB_PORT = 8080;
    constexpr size_t SHA1_HASH_SIZE = 20;
    constexpr size_t MAX_BUCKET_SIZE = 16;
    constexpr size_t DEFAULT_ALPHA = 3;
    constexpr size_t DEFAULT_MAX_RESULTS = 8;
    constexpr int DEFAULT_TOKEN_ROTATION_INTERVAL = 300;
    constexpr int DEFAULT_BUCKET_REFRESH_INTERVAL = 60;
}

} // namespace common

//-----------------------------------------------------------------------------
// String utilities
//-----------------------------------------------------------------------------
namespace string {

/**
 * @brief Converts a byte array to a hex string
 * @param data Pointer to the input data
 * @param length Length of the input data
 * @return The hex string representation
 */
std::string bytesToHex(const uint8_t* data, size_t length);

/**
 * @brief Converts a hex string to a byte vector
 * @param hex The hex string
 * @return The byte vector
 */
std::vector<uint8_t> hexToBytes(const std::string& hex);

/**
 * @brief Formats a byte array as a hex string with optional separator
 * @param data Pointer to the input data
 * @param length Length of the input data
 * @param separator Optional separator between bytes (e.g., ":")
 * @return The formatted hex string
 */
std::string formatHex(const uint8_t* data, size_t length, const std::string& separator = "");

/**
 * @brief Truncates a string to a maximum length and adds an ellipsis if truncated
 * @param str The input string
 * @param maxLength The maximum length
 * @return The truncated string
 */
std::string truncateString(const std::string& str, size_t maxLength);

} // namespace string

//-----------------------------------------------------------------------------
// Hash utilities
//-----------------------------------------------------------------------------
namespace hash {

/**
 * @brief Computes the SHA-1 hash of a byte array
 * @param data Pointer to the input data
 * @param length Length of the input data
 * @return The SHA-1 hash as a byte array
 */
std::vector<uint8_t> sha1(const uint8_t* data, size_t length);

/**
 * @brief Computes the SHA-1 hash of a string
 * @param data The input string
 * @return The SHA-1 hash as a byte array
 */
std::vector<uint8_t> sha1(const std::string& data);

/**
 * @brief Computes the SHA-1 hash of a byte vector
 * @param data The input byte vector
 * @return The SHA-1 hash as a byte array
 */
std::vector<uint8_t> sha1(const std::vector<uint8_t>& data);

/**
 * @brief Computes the SHA-1 hash of a byte array and returns it as a hex string
 * @param data Pointer to the input data
 * @param length Length of the input data
 * @return The SHA-1 hash as a hex string
 */
std::string sha1Hex(const uint8_t* data, size_t length);

/**
 * @brief Computes the SHA-1 hash of a string and returns it as a hex string
 * @param data The input string
 * @return The SHA-1 hash as a hex string
 */
std::string sha1Hex(const std::string& data);

/**
 * @brief Computes the SHA-1 hash of a byte vector and returns it as a hex string
 * @param data The input byte vector
 * @return The SHA-1 hash as a hex string
 */
std::string sha1Hex(const std::vector<uint8_t>& data);

} // namespace hash

//-----------------------------------------------------------------------------
// File utilities
//-----------------------------------------------------------------------------
namespace file {

// Import the Result class into this namespace for backward compatibility
using common::Result;

/**
 * @brief Checks if a file exists
 * @param filePath The file path
 * @return True if the file exists, false otherwise
 */
bool fileExists(const std::string& filePath);

/**
 * @brief Creates a directory if it doesn't exist
 * @param dirPath The directory path
 * @return True if the directory was created or already exists, false otherwise
 */
bool createDirectory(const std::string& dirPath);

/**
 * @brief Gets the file size
 * @param filePath The file path
 * @return The file size in bytes, or 0 if the file doesn't exist
 */
size_t getFileSize(const std::string& filePath);

/**
 * @brief Reads a file into a string
 * @param filePath The file path
 * @return Result containing the file content or an error message
 */
Result<std::string> readFile(const std::string& filePath);

/**
 * @brief Writes a string to a file
 * @param filePath The file path
 * @param content The content to write
 * @return Result indicating success or failure with an error message
 */
Result<void> writeFile(const std::string& filePath, const std::string& content);

} // namespace file

//-----------------------------------------------------------------------------
// Filesystem utilities
//-----------------------------------------------------------------------------
namespace filesystem {

// Import the Result class into this namespace for backward compatibility
using common::Result;

/**
 * @brief Gets all files in a directory
 * @param dirPath The directory path
 * @param recursive Whether to search recursively
 * @return Result containing a vector of file paths or an error message
 */
Result<std::vector<std::string>> getFiles(const std::string& dirPath, bool recursive = false);

/**
 * @brief Gets all directories in a directory
 * @param dirPath The directory path
 * @param recursive Whether to search recursively
 * @return Result containing a vector of directory paths or an error message
 */
Result<std::vector<std::string>> getDirectories(const std::string& dirPath, bool recursive = false);

/**
 * @brief Gets the file extension
 * @param filePath The file path
 * @return The file extension (without the dot)
 */
std::string getFileExtension(const std::string& filePath);

/**
 * @brief Gets the file name without extension
 * @param filePath The file path
 * @return The file name without extension
 */
std::string getFileName(const std::string& filePath);

/**
 * @brief Gets the directory path from a file path
 * @param filePath The file path
 * @return The directory path
 */
std::string getDirectoryPath(const std::string& filePath);

} // namespace filesystem

} // namespace dht_hunter::utility
