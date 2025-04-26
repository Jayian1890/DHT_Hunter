#pragma once

#include "dht_hunter/network/address/address_family.hpp"
#include <array>
#include <cstdint>
#include <string>

namespace dht_hunter::network {

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

} // namespace dht_hunter::network
