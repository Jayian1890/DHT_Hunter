#pragma once

#include "dht_hunter/network/socket/socket.hpp"
#include "dht_hunter/network/socket/udp_socket.hpp"
#include "dht_hunter/network/address/network_address.hpp"
#include "dht_hunter/network/address/endpoint.hpp"
#include <cstdint>
#include <string>

namespace dht_hunter::network {

/**
 * @class SocketImpl
 * @brief Unix implementation of the Socket interface.
 *
 * This class provides a Unix-specific implementation of the Socket interface.
 */
class SocketImpl : public Socket {
public:
    /**
     * @brief Constructs a socket.
     * @param type The socket type.
     * @param family The address family.
     */
    SocketImpl(SocketType type, AddressFamily family);

    /**
     * @brief Destructor.
     */
    virtual ~SocketImpl();

    /**
     * @brief Binds the socket to a local address and port.
     * @param address The local address to bind to.
     * @param port The local port to bind to.
     * @return True if the socket was successfully bound, false otherwise.
     */
    bool bind(const NetworkAddress& address, uint16_t port) override;

    /**
     * @brief Closes the socket.
     */
    void close() override;

    /**
     * @brief Checks if the socket is valid.
     * @return True if the socket is valid, false otherwise.
     */
    bool isValid() const override;

    /**
     * @brief Gets the local endpoint of the socket.
     * @return The local endpoint.
     */
    EndPoint getLocalEndpoint() const override;

    /**
     * @brief Gets the socket type.
     * @return The socket type.
     */
    SocketType getType() const override;

    /**
     * @brief Sets the socket to non-blocking mode.
     * @param nonBlocking True to set the socket to non-blocking mode, false otherwise.
     * @return True if the operation was successful, false otherwise.
     */
    bool setNonBlocking(bool nonBlocking) override;

    /**
     * @brief Sets the receive buffer size.
     * @param size The receive buffer size in bytes.
     * @return True if the operation was successful, false otherwise.
     */
    bool setReceiveBufferSize(int size) override;

    /**
     * @brief Sets the send buffer size.
     * @param size The send buffer size in bytes.
     * @return True if the operation was successful, false otherwise.
     */
    bool setSendBufferSize(int size) override;

    /**
     * @brief Sets the receive timeout.
     * @param timeoutMs The receive timeout in milliseconds.
     * @return True if the operation was successful, false otherwise.
     */
    bool setReceiveTimeout(int timeoutMs) override;

    /**
     * @brief Sets the send timeout.
     * @param timeoutMs The send timeout in milliseconds.
     * @return True if the operation was successful, false otherwise.
     */
    bool setSendTimeout(int timeoutMs) override;

    /**
     * @brief Sets the reuse address option.
     * @param reuse True to enable the reuse address option, false otherwise.
     * @return True if the operation was successful, false otherwise.
     */
    bool setReuseAddress(bool reuse) override;

    /**
     * @brief Gets the last socket error.
     * @return The last socket error.
     */
    SocketError getLastError() const override;

    /**
     * @brief Gets the error message for the last socket error.
     * @return The error message.
     */
    std::string getLastErrorMessage() const override;

    /**
     * @brief Gets the socket descriptor.
     * @return The socket descriptor.
     */
    int getSocketDescriptor() const;

protected:
    int m_socket;
    SocketType m_type;
    AddressFamily m_family;
    mutable SocketError m_lastError;
};

/**
 * @class UDPSocketImpl
 * @brief Unix implementation of the UDPSocket interface.
 *
 * This class provides a Unix-specific implementation of the UDPSocket interface.
 */
class UDPSocketImpl : public SocketImpl, public UDPSocket {
public:
    /**
     * @brief Constructs a UDP socket.
     * @param family The address family.
     */
    explicit UDPSocketImpl(AddressFamily family = AddressFamily::IPv4);

    /**
     * @brief Destructor.
     */
    virtual ~UDPSocketImpl();

    /**
     * @brief Sends a datagram to a remote endpoint.
     * @param data The data to send.
     * @param size The size of the data in bytes.
     * @param destination The remote endpoint to send to.
     * @return The number of bytes sent, or -1 if an error occurred.
     */
    int sendTo(const void* data, int size, const EndPoint& destination) override;

    /**
     * @brief Receives a datagram from a remote endpoint.
     * @param buffer The buffer to receive the data.
     * @param size The size of the buffer in bytes.
     * @param source The remote endpoint that sent the data (output).
     * @return The number of bytes received, or -1 if an error occurred.
     */
    int receiveFrom(void* buffer, int size, EndPoint& source) override;

    /**
     * @brief Joins a multicast group.
     * @param groupAddress The multicast group address.
     * @param interfaceAddress The interface address to use for the multicast group.
     * @return True if the operation was successful, false otherwise.
     */
    bool joinMulticastGroup(const NetworkAddress& groupAddress, const NetworkAddress& interfaceAddress = NetworkAddress()) override;

    /**
     * @brief Leaves a multicast group.
     * @param groupAddress The multicast group address.
     * @param interfaceAddress The interface address used for the multicast group.
     * @return True if the operation was successful, false otherwise.
     */
    bool leaveMulticastGroup(const NetworkAddress& groupAddress, const NetworkAddress& interfaceAddress = NetworkAddress()) override;

    /**
     * @brief Sets the multicast loopback option.
     * @param loopback True to enable multicast loopback, false otherwise.
     * @return True if the operation was successful, false otherwise.
     */
    bool setMulticastLoopback(bool loopback) override;

    /**
     * @brief Sets the multicast time-to-live (TTL).
     * @param ttl The multicast TTL.
     * @return True if the operation was successful, false otherwise.
     */
    bool setMulticastTTL(int ttl) override;
};

} // namespace dht_hunter::network
