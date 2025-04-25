#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace dht_hunter::network {

/**
 * @enum AddressFamily
 * @brief Defines the address family (IPv4 or IPv6).
 */
enum class AddressFamily {
    IPv4,
    IPv6,
    Unspecified
};

/**
 * @class NetworkAddress
 * @brief Represents an IP address (IPv4 or IPv6).
 *
 * This class provides a unified interface for handling both IPv4 and IPv6 addresses,
 * with conversion methods between string and binary representations.
 */
class NetworkAddress {
public:
    /**
     * @brief Default constructor. Creates an unspecified address.
     */
    NetworkAddress();

    /**
     * @brief Constructs an IPv4 address from a 32-bit integer.
     * @param ipv4Address The IPv4 address as a 32-bit integer in host byte order.
     */
    explicit NetworkAddress(uint32_t ipv4Address);

    /**
     * @brief Constructs an IPv6 address from a 16-byte array.
     * @param ipv6Address The IPv6 address as a 16-byte array.
     */
    explicit NetworkAddress(const std::array<uint8_t, 16>& ipv6Address);

    /**
     * @brief Constructs an address from a string representation.
     * @param addressStr The string representation of the address (e.g., "192.168.1.1" or "2001:db8::1").
     */
    explicit NetworkAddress(const std::string& addressStr);

    /**
     * @brief Gets the address family (IPv4 or IPv6).
     * @return The address family.
     */
    AddressFamily getFamily() const;

    /**
     * @brief Gets the IPv4 address as a 32-bit integer.
     * @return The IPv4 address in host byte order.
     * @throws std::runtime_error if the address is not IPv4.
     */
    uint32_t getIPv4Address() const;

    /**
     * @brief Gets the IPv6 address as a 16-byte array.
     * @return The IPv6 address.
     * @throws std::runtime_error if the address is not IPv6.
     */
    const std::array<uint8_t, 16>& getIPv6Address() const;

    /**
     * @brief Gets the string representation of the address.
     * @return The string representation of the address.
     */
    std::string toString() const;

    /**
     * @brief Sets the address from a string representation.
     * @param addressStr The string representation of the address (e.g., "192.168.1.1" or "2001:db8::1").
     * @return True if the address was successfully set, false otherwise.
     */
    bool fromString(const std::string& addressStr);

    /**
     * @brief Checks if the address is valid.
     * @return True if the address is valid, false otherwise.
     */
    bool isValid() const;

    /**
     * @brief Checks if the address is a loopback address.
     * @return True if the address is a loopback address, false otherwise.
     */
    bool isLoopback() const;

    /**
     * @brief Checks if the address is a multicast address.
     * @return True if the address is a multicast address, false otherwise.
     */
    bool isMulticast() const;

    /**
     * @brief Checks if the address is an IPv4 address.
     * @return True if the address is an IPv4 address, false otherwise.
     */
    bool isIPv4() const;

    /**
     * @brief Checks if the address is an IPv6 address.
     * @return True if the address is an IPv6 address, false otherwise.
     */
    bool isIPv6() const;

    /**
     * @brief Gets the binary representation of the address.
     * @return The binary representation of the address.
     */
    std::string toBytes() const;

    /**
     * @brief Creates a loopback address.
     * @param family The address family (IPv4 or IPv6).
     * @return A loopback address of the specified family.
     */
    static NetworkAddress loopback(AddressFamily family = AddressFamily::IPv4);

    /**
     * @brief Creates an any address (0.0.0.0 or ::).
     * @param family The address family (IPv4 or IPv6).
     * @return An any address of the specified family.
     */
    static NetworkAddress any(AddressFamily family = AddressFamily::IPv4);

    /**
     * @brief Equality operator.
     * @param other The other address to compare with.
     * @return True if the addresses are equal, false otherwise.
     */
    bool operator==(const NetworkAddress& other) const;

    /**
     * @brief Inequality operator.
     * @param other The other address to compare with.
     * @return True if the addresses are not equal, false otherwise.
     */
    bool operator!=(const NetworkAddress& other) const;

private:
    AddressFamily m_family;
    uint32_t m_ipv4Address;
    std::array<uint8_t, 16> m_ipv6Address;
};

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

/**
 * @class AddressResolver
 * @brief Resolves hostnames to IP addresses.
 *
 * This class provides methods for resolving hostnames to IP addresses
 * using DNS lookups.
 */
class AddressResolver {
public:
    /**
     * @brief Resolves a hostname to IP addresses.
     * @param hostname The hostname to resolve.
     * @param family The address family to resolve to (IPv4, IPv6, or both).
     * @return A vector of resolved IP addresses.
     */
    static std::vector<NetworkAddress> resolve(const std::string& hostname,
                                              AddressFamily family = AddressFamily::Unspecified);

    /**
     * @brief Resolves a hostname and port to endpoints.
     * @param hostname The hostname to resolve.
     * @param port The port number.
     * @param family The address family to resolve to (IPv4, IPv6, or both).
     * @return A vector of resolved endpoints.
     */
    static std::vector<EndPoint> resolveEndpoint(const std::string& hostname,
                                               uint16_t port,
                                               AddressFamily family = AddressFamily::Unspecified);
};

} // namespace dht_hunter::network
