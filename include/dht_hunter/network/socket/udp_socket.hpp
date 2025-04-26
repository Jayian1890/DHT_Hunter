#pragma once

#include "dht_hunter/network/socket/socket.hpp"
#include "dht_hunter/network/address/network_address.hpp"
#include "dht_hunter/network/address/endpoint.hpp"
#include <cstdint>
#include <memory>
#include <string>

namespace dht_hunter::network {

/**
 * @class UDPSocket
 * @brief Interface for UDP socket operations.
 *
 * This class provides an interface for UDP socket operations, with support
 * for sending and receiving datagrams.
 */
class UDPSocket : public Socket {
public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~UDPSocket() = default;

    /**
     * @brief Sends a datagram to a remote endpoint.
     * @param data The data to send.
     * @param size The size of the data in bytes.
     * @param destination The remote endpoint to send to.
     * @return The number of bytes sent, or -1 if an error occurred.
     */
    virtual int sendTo(const void* data, int size, const EndPoint& destination) = 0;

    /**
     * @brief Receives a datagram from a remote endpoint.
     * @param buffer The buffer to receive the data.
     * @param size The size of the buffer in bytes.
     * @param source The remote endpoint that sent the data (output).
     * @return The number of bytes received, or -1 if an error occurred.
     */
    virtual int receiveFrom(void* buffer, int size, EndPoint& source) = 0;

    /**
     * @brief Joins a multicast group.
     * @param groupAddress The multicast group address.
     * @param interfaceAddress The interface address to use for the multicast group.
     * @return True if the operation was successful, false otherwise.
     */
    virtual bool joinMulticastGroup(const NetworkAddress& groupAddress, const NetworkAddress& interfaceAddress = NetworkAddress()) = 0;

    /**
     * @brief Leaves a multicast group.
     * @param groupAddress The multicast group address.
     * @param interfaceAddress The interface address used for the multicast group.
     * @return True if the operation was successful, false otherwise.
     */
    virtual bool leaveMulticastGroup(const NetworkAddress& groupAddress, const NetworkAddress& interfaceAddress = NetworkAddress()) = 0;

    /**
     * @brief Sets the multicast loopback option.
     * @param loopback True to enable multicast loopback, false otherwise.
     * @return True if the operation was successful, false otherwise.
     */
    virtual bool setMulticastLoopback(bool loopback) = 0;

    /**
     * @brief Sets the multicast time-to-live (TTL).
     * @param ttl The multicast TTL.
     * @return True if the operation was successful, false otherwise.
     */
    virtual bool setMulticastTTL(int ttl) = 0;

    /**
     * @brief Creates a UDP socket.
     * @param family The address family.
     * @return A shared pointer to the created UDP socket.
     */
    static std::shared_ptr<UDPSocket> create(AddressFamily family = AddressFamily::IPv4);
};

} // namespace dht_hunter::network
