#pragma once

#include "dht_hunter/dht/core/dht_node_config.hpp"
#include "dht_hunter/network/socket.hpp"
#include "dht_hunter/network/socket_factory.hpp"
#include "dht_hunter/util/mutex_utils.hpp"
#include <memory>
#include <thread>
#include <atomic>
#include <functional>

namespace dht_hunter::dht {

/**
 * @brief Callback for received messages
 * @param data The received data
 * @param size The size of the received data
 * @param sender The sender's endpoint
 */
using MessageReceivedCallback = std::function<void(const uint8_t* data, size_t size, const network::EndPoint& sender)>;

/**
 * @brief Manages the UDP socket for DHT communication
 */
class DHTSocketManager {
public:
    /**
     * @brief Constructs a socket manager
     * @param config The DHT node configuration
     */
    explicit DHTSocketManager(const DHTNodeConfig& config);

    /**
     * @brief Destructor
     */
    ~DHTSocketManager();

    /**
     * @brief Starts the socket manager
     * @param messageCallback The callback to call when a message is received
     * @return True if the socket manager was started successfully, false otherwise
     */
    bool start(MessageReceivedCallback messageCallback);

    /**
     * @brief Stops the socket manager
     */
    void stop();

    /**
     * @brief Checks if the socket manager is running
     * @return True if the socket manager is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Sends a message
     * @param data The data to send
     * @param size The size of the data
     * @param endpoint The endpoint to send the data to
     * @param callback The callback to call when the message is sent
     * @return True if the message was queued for sending, false otherwise
     */
    bool sendMessage(const uint8_t* data, size_t size, const network::EndPoint& endpoint, 
                    std::function<void(bool)> callback = nullptr);

    /**
     * @brief Gets the local port
     * @return The local port
     */
    uint16_t getPort() const;

private:
    /**
     * @brief Receives messages
     */
    void receiveMessages();

    DHTNodeConfig m_config;
    uint16_t m_port;
    std::unique_ptr<network::Socket> m_socket;
    util::CheckedMutex m_socketMutex;
    std::atomic<bool> m_running{false};
    std::thread m_receiveThread;
    MessageReceivedCallback m_messageCallback;
};

} // namespace dht_hunter::dht
