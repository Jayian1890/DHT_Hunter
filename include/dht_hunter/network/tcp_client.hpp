#pragma once

#include "dht_hunter/network/network_address.hpp"
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <deque>

namespace dht_hunter::network {

/**
 * @brief Connection statistics for monitoring connection quality
 */
struct ConnectionStats {
    std::chrono::steady_clock::time_point connectTime;       // When the connection was established
    std::chrono::steady_clock::time_point lastActivityTime;  // Last time data was sent or received
    std::chrono::milliseconds connectDuration{0};           // How long it took to establish the connection
    size_t bytesSent{0};                                    // Total bytes sent
    size_t bytesReceived{0};                               // Total bytes received
    int errorCount{0};                                      // Number of errors encountered
    int reconnectCount{0};                                  // Number of reconnection attempts
    float latencyMs{0.0f};                                  // Average latency in milliseconds

    // Reset statistics but keep history
    void reset() {
        connectTime = std::chrono::steady_clock::now();
        lastActivityTime = connectTime;
        connectDuration = std::chrono::milliseconds(0);
        bytesSent = 0;
        bytesReceived = 0;
        errorCount = 0;
    }
};

/**
 * @brief A TCP client with enhanced connection management
 */
class TCPClient {
public:
    /**
     * @brief Constructor
     */
    TCPClient();

    /**
     * @brief Destructor
     */
    ~TCPClient();

    /**
     * @brief Connects to a server with configurable timeout
     * @param ip The IP address
     * @param port The port
     * @param timeoutSec Connection timeout in seconds (default: 5)
     * @return True if the connection was established, false otherwise
     */
    bool connect(const std::string& ip, uint16_t port, int timeoutSec = 5);

    /**
     * @brief Gets the connection statistics
     * @return The connection statistics
     */
    const ConnectionStats& getStats() const;

    /**
     * @brief Disconnects from the server
     */
    void disconnect();

    /**
     * @brief Checks if the client is connected
     * @return True if connected, false otherwise
     */
    bool isConnected() const;

    /**
     * @brief Sends data to the server
     * @param data The data to send
     * @param length The length of the data
     * @return True if the data was sent, false otherwise
     */
    bool send(const uint8_t* data, size_t length);

    /**
     * @brief Sets the data received callback
     * @param callback The callback
     */
    void setDataReceivedCallback(std::function<void(const uint8_t*, size_t)> callback);

    /**
     * @brief Sets the error callback
     * @param callback The callback
     */
    void setErrorCallback(std::function<void(const std::string&)> callback);

    /**
     * @brief Sets the connection closed callback
     * @param callback The callback
     */
    void setConnectionClosedCallback(std::function<void()> callback);

private:
    /**
     * @brief Receives data from the server
     */
    void receiveData();

    /**
     * @brief Handles an error
     * @param error The error message
     */
    void handleError(const std::string& error);

    /**
     * @brief Handles a connection closed
     */
    void handleConnectionClosed();

    /**
     * @brief Updates connection statistics with received data
     * @param bytesCount Number of bytes received
     */
    void updateStatsForDataReceived(size_t bytesCount);

    /**
     * @brief Updates connection statistics with sent data
     * @param bytesCount Number of bytes sent
     */
    void updateStatsForDataSent(size_t bytesCount);

    /**
     * @brief Updates latency measurement
     * @param latencyMs Latency in milliseconds
     */
    void updateLatency(float latencyMs);

    // Socket
    int m_socket;

    // Connection state
    std::atomic<bool> m_connected;

    // Receive thread
    std::thread m_receiveThread;
    std::atomic<bool> m_running;

    // Callbacks
    std::function<void(const uint8_t*, size_t)> m_dataReceivedCallback;
    std::function<void(const std::string&)> m_errorCallback;
    std::function<void()> m_connectionClosedCallback;
    std::mutex m_callbackMutex;

    // Buffer
    std::vector<uint8_t> m_receiveBuffer;

    // Connection statistics
    ConnectionStats m_stats;
    std::mutex m_statsMutex;

    // Latency measurement
    std::deque<float> m_latencyHistory;
    static constexpr size_t MAX_LATENCY_HISTORY = 10;
};

} // namespace dht_hunter::network
