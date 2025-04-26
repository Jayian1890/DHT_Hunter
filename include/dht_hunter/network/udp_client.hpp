#pragma once

#include "dht_hunter/network/udp_socket.hpp"
#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <sys/types.h>  // for ssize_t

namespace dht_hunter::network {

/**
 * @class UDPClient
 * @brief A simple UDP client for sending and receiving datagrams.
 */
class UDPClient {
public:
    /**
     * @brief Default constructor.
     */
    UDPClient();

    /**
     * @brief Destructor.
     */
    ~UDPClient();

    /**
     * @brief Start the client.
     * @return True if successful, false otherwise.
     */
    bool start();

    /**
     * @brief Stop the client.
     */
    void stop();

    /**
     * @brief Check if the client is running.
     * @return True if the client is running, false otherwise.
     */
    bool isRunning() const;

    /**
     * @brief Send data to a specific server.
     * @param data The data to send.
     * @param address The server's address.
     * @param port The server's port.
     * @return The number of bytes sent, or -1 on error.
     */
    ssize_t send(const std::vector<uint8_t>& data, const std::string& address, uint16_t port);

    /**
     * @brief Set a callback function to be called when data is received.
     * @param callback The callback function.
     */
    void setMessageHandler(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback);

private:
    UDPSocket m_socket;
    bool m_running;
};

} // namespace dht_hunter::network
