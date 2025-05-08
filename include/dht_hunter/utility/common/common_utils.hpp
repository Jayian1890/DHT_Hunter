#pragma once

// Common utility functions and types for the DHT Hunter project
// This file consolidates common functionality from common.hpp

// Standard C++ libraries (most common ones)
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace dht_hunter::utility::common {

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

} // namespace dht_hunter::utility::common
