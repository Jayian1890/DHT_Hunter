#pragma once

#include "dht_hunter/network/network_address.hpp"
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>

namespace dht_hunter::network {

/**
 * @brief A simple TCP client
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
     * @brief Connects to a server
     * @param ip The IP address
     * @param port The port
     * @return True if the connection was established, false otherwise
     */
    bool connect(const std::string& ip, uint16_t port);

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
};

} // namespace dht_hunter::network
