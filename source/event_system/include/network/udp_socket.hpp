#pragma once

#include "network/network_interface.hpp"
#include "event_system/event_bus.hpp"
#include "event_system/common_events.hpp"
#include "utils/async_utils.hpp"
#include <atomic>
#include <mutex>
#include <thread>
#include <future>

namespace network {

/**
 * @brief UDP socket configuration
 */
struct UDPSocketConfig {
    uint16_t port = 6881;                  // Local port (0 for any)
    size_t receiveBufferSize = 65536;   // Receive buffer size
    bool reuseAddress = true;           // Whether to reuse the address
    bool broadcast = false;             // Whether to enable broadcasting
};

/**
 * @brief UDP socket class
 */
class UDPSocket : public NetworkInterface {
public:
    /**
     * @brief Constructor
     * @param config The socket configuration
     */
    explicit UDPSocket(const UDPSocketConfig& config = UDPSocketConfig());

    /**
     * @brief Destructor
     */
    ~UDPSocket() override;

    /**
     * @brief Opens the socket
     * @return A future that will be fulfilled when the operation completes
     */
    std::future<bool> open() override;

    /**
     * @brief Closes the socket
     * @return A future that will be fulfilled when the operation completes
     */
    std::future<void> close() override;

    /**
     * @brief Sends a message
     * @param address The destination address
     * @param message The message to send
     * @return A future that will be fulfilled when the operation completes
     */
    std::future<bool> send(const NetworkAddress& address, const NetworkMessage& message) override;

    /**
     * @brief Checks if the socket is open
     * @return True if the socket is open, false otherwise
     */
    bool isOpen() const override;

    /**
     * @brief Gets the local address
     * @return The local address
     */
    NetworkAddress getLocalAddress() const;

private:
    /**
     * @brief Receives data from the socket
     */
    void receiveLoop();

    UDPSocketConfig m_config;
    int m_socket;
    std::atomic<bool> m_running;
    std::future<void> m_receiveThread;
    mutable std::mutex m_socketMutex;
    NetworkAddress m_localAddress;
};

} // namespace network
