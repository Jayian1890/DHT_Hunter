#include "dht_hunter/network/network_address.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sstream>

namespace dht_hunter::network {

NetworkAddress::NetworkAddress(const std::string& address)
    : m_address(address), m_isIPv4(false), m_isValid(false) {
    // Check if the address is IPv4
    struct in_addr ipv4Addr;
    if (inet_pton(AF_INET, address.c_str(), &ipv4Addr) == 1) {
        m_isIPv4 = true;
        m_isValid = true;
        return;
    }

    // Check if the address is IPv6
    struct in6_addr ipv6Addr;
    if (inet_pton(AF_INET6, address.c_str(), &ipv6Addr) == 1) {
        m_isIPv4 = false;
        m_isValid = true;
        return;
    }

    // Invalid address
    m_isValid = false;
}

NetworkAddress::NetworkAddress(const std::vector<uint8_t>& bytes)
    : m_isIPv4(false), m_isValid(false) {
    // Check if the bytes represent an IPv4 address
    if (bytes.size() == 4) {
        char ipStr[INET_ADDRSTRLEN];
        struct in_addr addr;
        std::memcpy(&addr.s_addr, bytes.data(), 4);
        inet_ntop(AF_INET, &addr, ipStr, INET_ADDRSTRLEN);
        m_address = ipStr;
        m_isIPv4 = true;
        m_isValid = true;
        return;
    }

    // Check if the bytes represent an IPv6 address
    if (bytes.size() == 16) {
        char ipStr[INET6_ADDRSTRLEN];
        struct in6_addr addr;
        std::memcpy(&addr.s6_addr, bytes.data(), 16);
        inet_ntop(AF_INET6, &addr, ipStr, INET6_ADDRSTRLEN);
        m_address = ipStr;
        m_isIPv4 = false;
        m_isValid = true;
        return;
    }

    // Invalid address
    m_isValid = false;
}

std::string NetworkAddress::toString() const {
    return m_address;
}

std::vector<uint8_t> NetworkAddress::toBytes() const {
    std::vector<uint8_t> bytes;

    if (m_isIPv4) {
        // Convert IPv4 address to bytes
        struct in_addr addr;
        if (inet_pton(AF_INET, m_address.c_str(), &addr) == 1) {
            bytes.resize(4);
            std::memcpy(bytes.data(), &addr.s_addr, 4);
        }
    } else {
        // Convert IPv6 address to bytes
        struct in6_addr addr;
        if (inet_pton(AF_INET6, m_address.c_str(), &addr) == 1) {
            bytes.resize(16);
            std::memcpy(bytes.data(), &addr.s6_addr, 16);
        }
    }

    return bytes;
}

bool NetworkAddress::isValid() const {
    return m_isValid;
}

bool NetworkAddress::isIPv4() const {
    return m_isIPv4;
}

bool NetworkAddress::isIPv6() const {
    return m_isValid && !m_isIPv4;
}

bool NetworkAddress::operator==(const NetworkAddress& other) const {
    return m_address == other.m_address;
}

bool NetworkAddress::operator!=(const NetworkAddress& other) const {
    return !(*this == other);
}

EndPoint::EndPoint(const NetworkAddress& address, uint16_t port)
    : m_address(address), m_port(port) {
}

const NetworkAddress& EndPoint::getAddress() const {
    return m_address;
}

uint16_t EndPoint::getPort() const {
    return m_port;
}

std::string EndPoint::toString() const {
    std::stringstream ss;
    if (m_address.isIPv6()) {
        ss << "[" << m_address.toString() << "]:" << m_port;
    } else {
        ss << m_address.toString() << ":" << m_port;
    }
    return ss.str();
}

bool EndPoint::operator==(const EndPoint& other) const {
    return m_address == other.m_address && m_port == other.m_port;
}

bool EndPoint::operator!=(const EndPoint& other) const {
    return !(*this == other);
}

} // namespace dht_hunter::network
