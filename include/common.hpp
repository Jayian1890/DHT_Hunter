#pragma once

// This is a precompiled header file that includes common standard libraries
// and project-specific headers to reduce compilation time.

// Standard C++ libraries
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Filesystem support
#include <filesystem>

// Platform-specific headers
#ifdef _WIN32
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif

#ifdef __APPLE__
    #include <CoreFoundation/CoreFoundation.h>
    #include <IOKit/IOKitLib.h>
    #include <IOKit/pwr_mgt/IOPMLib.h>
    #include <IOKit/IOMessage.h>
#endif

// Common project-specific types
namespace dht_hunter {

// Include common types
// Forward declarations of common types
namespace types {
    // Note: InfoHash is defined in dht_hunter/types/info_hash.hpp
    class NodeID;
    class EndPoint;
    class InfoHashMetadata;
}

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

// Common utility functions
namespace util {
    // String conversion utilities
    template<typename T>
    inline std::string toString(const T& value) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }

    // Hex conversion utilities
    inline std::string bytesToHex(const uint8_t* data, size_t length) {
        static const char* hexChars = "0123456789abcdef";
        std::string result;
        result.reserve(length * 2);

        for (size_t i = 0; i < length; ++i) {
            result.push_back(hexChars[(data[i] >> 4) & 0xF]);
            result.push_back(hexChars[data[i] & 0xF]);
        }

        return result;
    }

    inline std::vector<uint8_t> hexToBytes(const std::string& hex) {
        std::vector<uint8_t> result;
        result.reserve(hex.length() / 2);

        for (size_t i = 0; i < hex.length(); i += 2) {
            uint8_t byte = 0;
            if (i + 1 < hex.length()) {
                byte = static_cast<uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16));
            }
            result.push_back(byte);
        }

        return result;
    }

    // Random number generation
    inline std::mt19937& getRandomGenerator() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        return gen;
    }

    template<typename T>
    inline T getRandomNumber(T min, T max) {
        std::uniform_int_distribution<T> dist(min, max);
        return dist(getRandomGenerator());
    }

    // UUID generation
    inline std::string generateUUID() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        static const char* digits = "0123456789abcdef";

        std::string uuid;
        uuid.reserve(36);

        for (int i = 0; i < 36; ++i) {
            if (i == 8 || i == 13 || i == 18 || i == 23) {
                uuid += '-';
            } else if (i == 14) {
                uuid += '4';
            } else if (i == 19) {
                uuid += digits[(dis(gen) & 0x3) | 0x8];
            } else {
                uuid += digits[dis(gen)];
            }
        }

        return uuid;
    }
}

// Common result type for operations that can fail
template<typename T>
class Result {
public:
    Result(const T& value) : m_value(value), m_success(true), m_error("") {}
    Result(T&& value) : m_value(::std::move(value)), m_success(true), m_error("") {}
    Result(const ::std::string& error) : m_success(false), m_error(error) {}

    bool isSuccess() const { return m_success; }
    bool isError() const { return !m_success; }
    const ::std::string& getError() const { return m_error; }
    const T& getValue() const { return m_value; }
    T& getValue() { return m_value; }

private:
    T m_value;
    bool m_success;
    ::std::string m_error;
};

// Specialization for void results
template<>
class Result<void> {
public:
    Result() : m_success(true), m_error("") {}
    Result(const ::std::string& error) : m_success(false), m_error(error) {}

    bool isSuccess() const { return m_success; }
    bool isError() const { return !m_success; }
    const ::std::string& getError() const { return m_error; }

private:
    bool m_success;
    ::std::string m_error;
};

} // namespace dht_hunter
