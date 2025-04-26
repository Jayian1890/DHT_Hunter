#pragma once

#include "dht_hunter/network/address/network_address.hpp"
#include "dht_hunter/network/address/endpoint.hpp"
#include "dht_hunter/network/socket/socket_error.hpp"
#include "dht_hunter/network/socket/socket_type.hpp"
#include <cstdint>
#include <memory>
#include <string>

namespace dht_hunter::network {

/**
 * @class Socket
 * @brief Base class for socket operations.
 *
 * This class provides an interface for socket operations, with support for
 * both TCP and UDP sockets.
 */
class Socket {
public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~Socket() = default;

    /**
     * @brief Binds the socket to a local address and port.
     * @param address The local address to bind to.
     * @param port The local port to bind to.
     * @return True if the socket was successfully bound, false otherwise.
     */
    virtual bool bind(const NetworkAddress& address, uint16_t port) = 0;

    /**
     * @brief Closes the socket.
     */
    virtual void close() = 0;

    /**
     * @brief Checks if the socket is valid.
     * @return True if the socket is valid, false otherwise.
     */
    virtual bool isValid() const = 0;

    /**
     * @brief Gets the local endpoint of the socket.
     * @return The local endpoint.
     */
    virtual EndPoint getLocalEndpoint() const = 0;

    /**
     * @brief Gets the socket type.
     * @return The socket type.
     */
    virtual SocketType getType() const = 0;

    /**
     * @brief Sets the socket to non-blocking mode.
     * @param nonBlocking True to set the socket to non-blocking mode, false otherwise.
     * @return True if the operation was successful, false otherwise.
     */
    virtual bool setNonBlocking(bool nonBlocking) = 0;

    /**
     * @brief Sets the receive buffer size.
     * @param size The receive buffer size in bytes.
     * @return True if the operation was successful, false otherwise.
     */
    virtual bool setReceiveBufferSize(int size) = 0;

    /**
     * @brief Sets the send buffer size.
     * @param size The send buffer size in bytes.
     * @return True if the operation was successful, false otherwise.
     */
    virtual bool setSendBufferSize(int size) = 0;

    /**
     * @brief Sets the receive timeout.
     * @param timeoutMs The receive timeout in milliseconds.
     * @return True if the operation was successful, false otherwise.
     */
    virtual bool setReceiveTimeout(int timeoutMs) = 0;

    /**
     * @brief Sets the send timeout.
     * @param timeoutMs The send timeout in milliseconds.
     * @return True if the operation was successful, false otherwise.
     */
    virtual bool setSendTimeout(int timeoutMs) = 0;

    /**
     * @brief Sets the reuse address option.
     * @param reuse True to enable the reuse address option, false otherwise.
     * @return True if the operation was successful, false otherwise.
     */
    virtual bool setReuseAddress(bool reuse) = 0;

    /**
     * @brief Gets the last socket error.
     * @return The last socket error.
     */
    virtual SocketError getLastError() const = 0;

    /**
     * @brief Gets the error message for the last socket error.
     * @return The error message.
     */
    virtual std::string getLastErrorMessage() const = 0;

    /**
     * @brief Creates a socket.
     * @param type The socket type.
     * @param family The address family.
     * @return A shared pointer to the created socket.
     */
    static std::shared_ptr<Socket> create(SocketType type, AddressFamily family = AddressFamily::IPv4);
};

} // namespace dht_hunter::network
