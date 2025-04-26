#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <cstdint>
#include <sys/types.h>  // for ssize_t

namespace dht_hunter::network {

/**
 * @class UDPSocket
 * @brief A simple UDP socket implementation for sending and receiving datagrams.
 */
class UDPSocket {
public:
    /**
     * @brief Default constructor.
     */
    UDPSocket();

    /**
     * @brief Destructor.
     */
    ~UDPSocket();

    /**
     * @brief Bind the socket to a specific port.
     * @param port The port to bind to.
     * @return True if successful, false otherwise.
     */
    bool bind(uint16_t port);

    /**
     * @brief Send data to a specific address and port.
     * @param data The data to send.
     * @param size The size of the data.
     * @param address The destination address.
     * @param port The destination port.
     * @return The number of bytes sent, or -1 on error.
     */
    ssize_t sendTo(const void* data, size_t size, const std::string& address, uint16_t port);

    /**
     * @brief Receive data from the socket.
     * @param buffer The buffer to store the received data.
     * @param size The size of the buffer.
     * @param address Output parameter for the sender's address.
     * @param port Output parameter for the sender's port.
     * @return The number of bytes received, or -1 on error.
     */
    ssize_t receiveFrom(void* buffer, size_t size, std::string& address, uint16_t& port);

    /**
     * @brief Set a callback function to be called when data is received.
     * @param callback The callback function.
     */
    void setReceiveCallback(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback);

    /**
     * @brief Start the receive loop in a separate thread.
     * @return True if successful, false otherwise.
     */
    bool startReceiveLoop();

    /**
     * @brief Stop the receive loop.
     */
    void stopReceiveLoop();

    /**
     * @brief Check if the socket is valid.
     * @return True if the socket is valid, false otherwise.
     */
    bool isValid() const;

    /**
     * @brief Close the socket.
     */
    void close();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace dht_hunter::network
