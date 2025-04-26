#pragma once

#include "dht_hunter/network/udp_socket.hpp"
#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <sys/types.h>  // for ssize_t

namespace dht_hunter::network {

/**
 * @class UDPServer
 * @brief A simple UDP server that listens for incoming datagrams.
 */
class UDPServer {
public:
    /**
     * @brief Default constructor.
     */
    UDPServer();

    /**
     * @brief Destructor.
     */
    ~UDPServer();

    /**
     * @brief Start the server on the specified port.
     * @param port The port to listen on.
     * @return True if successful, false otherwise.
     */
    bool start(uint16_t port);

    /**
     * @brief Stop the server.
     */
    void stop();

    /**
     * @brief Check if the server is running.
     * @return True if the server is running, false otherwise.
     */
    bool isRunning() const;

    /**
     * @brief Send data to a specific client.
     * @param data The data to send.
     * @param address The client's address.
     * @param port The client's port.
     * @return The number of bytes sent, or -1 on error.
     */
    ssize_t sendTo(const std::vector<uint8_t>& data, const std::string& address, uint16_t port);

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
