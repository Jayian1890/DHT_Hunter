#pragma once

#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/network/udp_socket.hpp"
#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/event/logger.hpp"
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

namespace dht_hunter::dht {

/**
 * @brief Manages the UDP socket for a DHT node
 */
class SocketManager {
public:
    /**
     * @brief Constructs a socket manager
     * @param config The DHT configuration
     */
    explicit SocketManager(const DHTConfig& config);

    /**
     * @brief Destructor
     */
    ~SocketManager();

    /**
     * @brief Starts the socket manager
     * @param receiveCallback The callback to call when data is received
     * @return True if the socket manager was started successfully, false otherwise
     */
    bool start(std::function<void(const uint8_t*, size_t, const network::EndPoint&)> receiveCallback);

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
     * @brief Gets the port
     * @return The port
     */
    uint16_t getPort() const;

    /**
     * @brief Sends data to an endpoint
     * @param data The data to send
     * @param size The size of the data
     * @param endpoint The endpoint to send to
     * @return The number of bytes sent, or -1 on error
     */
    ssize_t sendTo(const void* data, size_t size, const network::EndPoint& endpoint);

private:
    DHTConfig m_config;
    uint16_t m_port;
    std::atomic<bool> m_running;
    std::unique_ptr<network::UDPSocket> m_socket;
    std::mutex m_mutex;
    event::Logger m_logger;
};

} // namespace dht_hunter::dht
