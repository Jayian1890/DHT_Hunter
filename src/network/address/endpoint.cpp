#include "dht_hunter/network/address/endpoint.hpp"
#include <regex>
#include <stdexcept>

namespace dht_hunter::network {

EndPoint::EndPoint()
    : m_address(), m_port(0) {
}

EndPoint::EndPoint(const NetworkAddress& address, uint16_t port)
    : m_address(address), m_port(port) {
}

EndPoint::EndPoint(const std::string& endpointStr) {
    // Parse IPv4 endpoint (e.g., "192.168.1.1:8080")
    std::regex ipv4Regex("([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+):(\\d+)");
    std::smatch ipv4Match;
    if (std::regex_match(endpointStr, ipv4Match, ipv4Regex)) {
        m_address = NetworkAddress(ipv4Match[1].str());
        m_port = static_cast<uint16_t>(std::stoi(ipv4Match[2].str()));
        return;
    }

    // Parse IPv6 endpoint (e.g., "[2001:db8::1]:8080")
    std::regex ipv6Regex("\\[([^\\]]+)\\]:(\\d+)");
    std::smatch ipv6Match;
    if (std::regex_match(endpointStr, ipv6Match, ipv6Regex)) {
        m_address = NetworkAddress(ipv6Match[1].str());
        m_port = static_cast<uint16_t>(std::stoi(ipv6Match[2].str()));
        return;
    }

    // Invalid format
    throw std::invalid_argument("Invalid endpoint format: " + endpointStr);
}

const NetworkAddress& EndPoint::getAddress() const {
    return m_address;
}

uint16_t EndPoint::getPort() const {
    return m_port;
}

void EndPoint::setAddress(const NetworkAddress& address) {
    m_address = address;
}

void EndPoint::setPort(uint16_t port) {
    m_port = port;
}

std::string EndPoint::toString() const {
    if (!isValid()) {
        return "";
    }

    if (m_address.isIPv4()) {
        return m_address.toString() + ":" + std::to_string(m_port);
    } else if (m_address.isIPv6()) {
        return "[" + m_address.toString() + "]:" + std::to_string(m_port);
    } else {
        return "";
    }
}

bool EndPoint::isValid() const {
    return m_address.isValid() && m_port > 0;
}

bool EndPoint::operator==(const EndPoint& other) const {
    return m_address == other.m_address && m_port == other.m_port;
}

bool EndPoint::operator!=(const EndPoint& other) const {
    return !(*this == other);
}

} // namespace dht_hunter::network
