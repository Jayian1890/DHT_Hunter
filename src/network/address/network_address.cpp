#include "dht_hunter/network/address/network_address.hpp"
#include <arpa/inet.h>
#include <stdexcept>
#include <cstring>

namespace dht_hunter::network {

NetworkAddress::NetworkAddress()
    : m_family(AddressFamily::Unspecified), m_ipv4Address(0), m_ipv6Address{} {
}

NetworkAddress::NetworkAddress(uint32_t ipv4Address)
    : m_family(AddressFamily::IPv4), m_ipv4Address(ipv4Address), m_ipv6Address{} {
}

NetworkAddress::NetworkAddress(const std::array<uint8_t, 16>& ipv6Address)
    : m_family(AddressFamily::IPv6), m_ipv4Address(0), m_ipv6Address(ipv6Address) {
}

NetworkAddress::NetworkAddress(const std::string& addressStr)
    : m_family(AddressFamily::Unspecified), m_ipv4Address(0), m_ipv6Address{} {
    fromString(addressStr);
}

AddressFamily NetworkAddress::getFamily() const {
    return m_family;
}

uint32_t NetworkAddress::getIPv4Address() const {
    if (m_family != AddressFamily::IPv4) {
        throw std::runtime_error("Not an IPv4 address");
    }
    return m_ipv4Address;
}

const std::array<uint8_t, 16>& NetworkAddress::getIPv6Address() const {
    if (m_family != AddressFamily::IPv6) {
        throw std::runtime_error("Not an IPv6 address");
    }
    return m_ipv6Address;
}

std::string NetworkAddress::toString() const {
    char buffer[INET6_ADDRSTRLEN];

    if (m_family == AddressFamily::IPv4) {
        struct in_addr addr;
        addr.s_addr = htonl(m_ipv4Address);
        inet_ntop(AF_INET, &addr, buffer, INET_ADDRSTRLEN);
        return std::string(buffer);
    } else if (m_family == AddressFamily::IPv6) {
        struct in6_addr addr;
        std::memcpy(&addr, m_ipv6Address.data(), 16);
        inet_ntop(AF_INET6, &addr, buffer, INET6_ADDRSTRLEN);
        return std::string(buffer);
    } else {
        return "";
    }
}

bool NetworkAddress::fromString(const std::string& addressStr) {
    struct in_addr addr4;
    if (inet_pton(AF_INET, addressStr.c_str(), &addr4) == 1) {
        m_family = AddressFamily::IPv4;
        m_ipv4Address = ntohl(addr4.s_addr);
        return true;
    }

    struct in6_addr addr6;
    if (inet_pton(AF_INET6, addressStr.c_str(), &addr6) == 1) {
        m_family = AddressFamily::IPv6;
        std::memcpy(m_ipv6Address.data(), &addr6, 16);
        return true;
    }

    return false;
}

bool NetworkAddress::isValid() const {
    return m_family != AddressFamily::Unspecified;
}

bool NetworkAddress::isLoopback() const {
    if (m_family == AddressFamily::IPv4) {
        // 127.0.0.0/8
        return (m_ipv4Address & 0xFF000000) == 0x7F000000;
    } else if (m_family == AddressFamily::IPv6) {
        // ::1
        for (size_t i = 0; i < 15; ++i) {
            if (m_ipv6Address[i] != 0) {
                return false;
            }
        }
        return m_ipv6Address[15] == 1;
    } else {
        return false;
    }
}

bool NetworkAddress::isMulticast() const {
    if (m_family == AddressFamily::IPv4) {
        // 224.0.0.0/4
        return (m_ipv4Address & 0xF0000000) == 0xE0000000;
    } else if (m_family == AddressFamily::IPv6) {
        // ff00::/8
        return m_ipv6Address[0] == 0xFF;
    } else {
        return false;
    }
}

bool NetworkAddress::isIPv4() const {
    return m_family == AddressFamily::IPv4;
}

bool NetworkAddress::isIPv6() const {
    return m_family == AddressFamily::IPv6;
}

std::string NetworkAddress::toBytes() const {
    if (m_family == AddressFamily::IPv4) {
        std::string result(4, '\0');
        uint32_t addr = htonl(m_ipv4Address);
        std::memcpy(&result[0], &addr, 4);
        return result;
    } else if (m_family == AddressFamily::IPv6) {
        std::string result(16, '\0');
        std::memcpy(&result[0], m_ipv6Address.data(), 16);
        return result;
    } else {
        return "";
    }
}

NetworkAddress NetworkAddress::loopback(AddressFamily family) {
    if (family == AddressFamily::IPv4) {
        return NetworkAddress(0x7F000001); // 127.0.0.1
    } else if (family == AddressFamily::IPv6) {
        std::array<uint8_t, 16> addr{};
        addr[15] = 1; // ::1
        return NetworkAddress(addr);
    } else {
        return NetworkAddress();
    }
}

NetworkAddress NetworkAddress::any(AddressFamily family) {
    if (family == AddressFamily::IPv4) {
        return NetworkAddress(0); // 0.0.0.0
    } else if (family == AddressFamily::IPv6) {
        std::array<uint8_t, 16> addr{};
        return NetworkAddress(addr); // ::
    } else {
        return NetworkAddress();
    }
}

bool NetworkAddress::operator==(const NetworkAddress& other) const {
    if (m_family != other.m_family) {
        return false;
    }

    if (m_family == AddressFamily::IPv4) {
        return m_ipv4Address == other.m_ipv4Address;
    } else if (m_family == AddressFamily::IPv6) {
        return m_ipv6Address == other.m_ipv6Address;
    } else {
        return true; // Both unspecified
    }
}

bool NetworkAddress::operator!=(const NetworkAddress& other) const {
    return !(*this == other);
}

} // namespace dht_hunter::network
