#pragma once

#include "dht_hunter/network/address/network_address.hpp"
#include <string>

namespace dht_hunter::network {

/**
 * @class EndPoint
 * @brief Represents a network endpoint (IP address and port).
 *
 * This class combines a NetworkAddress with a port number to form a complete
 * endpoint for network communication.
 */
class EndPoint {
public:
    /**
     * @brief Default constructor. Creates an unspecified endpoint.
     */
    EndPoint();

    /**
     * @brief Constructs an endpoint from an address and port.
     * @param address The network address.
     * @param port The port number.
     */
    EndPoint(const NetworkAddress& address, uint16_t port);

    /**
     * @brief Constructs an endpoint from a string representation.
     * @param endpointStr The string representation of the endpoint (e.g., "192.168.1.1:8080" or "[2001:db8::1]:8080").
     */
    explicit EndPoint(const std::string& endpointStr);

    /**
     * @brief Gets the network address.
     * @return The network address.
     */
    const NetworkAddress& getAddress() const;

    /**
     * @brief Gets the port number.
     * @return The port number.
     */
    uint16_t getPort() const;

    /**
     * @brief Sets the network address.
     * @param address The network address.
     */
    void setAddress(const NetworkAddress& address);

    /**
     * @brief Sets the port number.
     * @param port The port number.
     */
    void setPort(uint16_t port);

    /**
     * @brief Gets the string representation of the endpoint.
     * @return The string representation of the endpoint.
     */
    std::string toString() const;

    /**
     * @brief Checks if the endpoint is valid.
     * @return True if the endpoint is valid, false otherwise.
     */
    bool isValid() const;

    /**
     * @brief Equality operator.
     * @param other The other endpoint to compare with.
     * @return True if the endpoints are equal, false otherwise.
     */
    bool operator==(const EndPoint& other) const;

    /**
     * @brief Inequality operator.
     * @param other The other endpoint to compare with.
     * @return True if the endpoints are not equal, false otherwise.
     */
    bool operator!=(const EndPoint& other) const;

private:
    NetworkAddress m_address;
    uint16_t m_port;
};

} // namespace dht_hunter::network
