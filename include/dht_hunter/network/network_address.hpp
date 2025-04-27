#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <arpa/inet.h>

namespace dht_hunter::network {

/**
 * @brief Represents a network address (IPv4 or IPv6)
 */
class NetworkAddress {
public:
    /**
     * @brief Default constructor
     * Creates an empty network address (0.0.0.0)
     */
    NetworkAddress();

    /**
     * @brief Constructs a network address from a string
     * @param address The address string (e.g., "192.168.1.1")
     */
    explicit NetworkAddress(const std::string& address);

    /**
     * @brief Constructs a network address from bytes
     * @param bytes The address bytes
     */
    explicit NetworkAddress(const std::vector<uint8_t>& bytes);

    /**
     * @brief Gets the string representation of the address
     * @return The address string
     */
    std::string toString() const;

    /**
     * @brief Gets the bytes representation of the address
     * @return The address bytes
     */
    std::vector<uint8_t> toBytes() const;

    /**
     * @brief Checks if the address is valid
     * @return True if the address is valid, false otherwise
     */
    bool isValid() const;

    /**
     * @brief Checks if the address is IPv4
     * @return True if the address is IPv4, false otherwise
     */
    bool isIPv4() const;

    /**
     * @brief Checks if the address is IPv6
     * @return True if the address is IPv6, false otherwise
     */
    bool isIPv6() const;

    /**
     * @brief Equality operator
     * @param other The other address
     * @return True if the addresses are equal, false otherwise
     */
    bool operator==(const NetworkAddress& other) const;

    /**
     * @brief Inequality operator
     * @param other The other address
     * @return True if the addresses are not equal, false otherwise
     */
    bool operator!=(const NetworkAddress& other) const;

private:
    std::string m_address;
    bool m_isIPv4;
    bool m_isValid;
};

/**
 * @brief Represents a network endpoint (address + port)
 */
class EndPoint {
public:
    /**
     * @brief Constructs an endpoint
     * @param address The network address
     * @param port The port
     */
    EndPoint(const NetworkAddress& address, uint16_t port);

    /**
     * @brief Gets the network address
     * @return The network address
     */
    const NetworkAddress& getAddress() const;

    /**
     * @brief Gets the port
     * @return The port
     */
    uint16_t getPort() const;

    /**
     * @brief Gets the string representation of the endpoint
     * @return The endpoint string
     */
    std::string toString() const;

    /**
     * @brief Equality operator
     * @param other The other endpoint
     * @return True if the endpoints are equal, false otherwise
     */
    bool operator==(const EndPoint& other) const;

    /**
     * @brief Inequality operator
     * @param other The other endpoint
     * @return True if the endpoints are not equal, false otherwise
     */
    bool operator!=(const EndPoint& other) const;

private:
    NetworkAddress m_address;
    uint16_t m_port;
};

} // namespace dht_hunter::network
