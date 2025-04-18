#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/logforge/logforge.hpp"

#include <stdexcept>
#include <regex>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
#endif

namespace dht_hunter::network {

namespace {
    // Get logger for network address
    auto logger = dht_hunter::logforge::LogForge::getLogger("NetworkAddress");
}

// NetworkAddress implementation

NetworkAddress::NetworkAddress()
    : m_family(AddressFamily::Unspecified),
      m_ipv4Address(0),
      m_ipv6Address{} {
}

NetworkAddress::NetworkAddress(uint32_t ipv4Address)
    : m_family(AddressFamily::IPv4),
      m_ipv4Address(ipv4Address),
      m_ipv6Address{} {
}

NetworkAddress::NetworkAddress(const std::array<uint8_t, 16>& ipv6Address)
    : m_family(AddressFamily::IPv6),
      m_ipv4Address(0),
      m_ipv6Address(ipv6Address) {
}

NetworkAddress::NetworkAddress(const std::string& addressStr)
    : m_family(AddressFamily::Unspecified),
      m_ipv4Address(0),
      m_ipv6Address{} {
    
    // Try to parse as IPv4
    struct in_addr addr4;
    if (inet_pton(AF_INET, addressStr.c_str(), &addr4) == 1) {
        m_family = AddressFamily::IPv4;
        m_ipv4Address = ntohl(addr4.s_addr);
        logger->debug("Parsed IPv4 address: {}", addressStr);
        return;
    }
    
    // Try to parse as IPv6
    struct in6_addr addr6;
    if (inet_pton(AF_INET6, addressStr.c_str(), &addr6) == 1) {
        m_family = AddressFamily::IPv6;
        std::memcpy(m_ipv6Address.data(), addr6.s6_addr, 16);
        logger->debug("Parsed IPv6 address: {}", addressStr);
        return;
    }
    
    logger->warning("Failed to parse address: {}", addressStr);
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
    if (m_family == AddressFamily::IPv4) {
        struct in_addr addr;
        addr.s_addr = htonl(m_ipv4Address);
        char buffer[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr, buffer, INET_ADDRSTRLEN);
        return std::string(buffer);
    } else if (m_family == AddressFamily::IPv6) {
        struct in6_addr addr;
        std::memcpy(addr.s6_addr, m_ipv6Address.data(), 16);
        char buffer[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &addr, buffer, INET6_ADDRSTRLEN);
        return std::string(buffer);
    } else {
        return "unspecified";
    }
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
    }
    return false;
}

bool NetworkAddress::isMulticast() const {
    if (m_family == AddressFamily::IPv4) {
        // 224.0.0.0/4
        return (m_ipv4Address & 0xF0000000) == 0xE0000000;
    } else if (m_family == AddressFamily::IPv6) {
        // ff00::/8
        return m_ipv6Address[0] == 0xFF;
    }
    return false;
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
        return NetworkAddress(std::array<uint8_t, 16>{}); // ::
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
    }
    
    return true;
}

bool NetworkAddress::operator!=(const NetworkAddress& other) const {
    return !(*this == other);
}

// EndPoint implementation

EndPoint::EndPoint()
    : m_address(),
      m_port(0) {
}

EndPoint::EndPoint(const NetworkAddress& address, uint16_t port)
    : m_address(address),
      m_port(port) {
}

EndPoint::EndPoint(const std::string& endpointStr)
    : m_address(),
      m_port(0) {
    
    auto logger = dht_hunter::logforge::LogForge::getLogger("EndPoint");
    
    // Parse IPv4 endpoint (e.g., "192.168.1.1:8080")
    std::regex ipv4Regex(R"(^(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}):(\d+)$)");
    std::smatch ipv4Match;
    if (std::regex_match(endpointStr, ipv4Match, ipv4Regex)) {
        m_address = NetworkAddress(ipv4Match[1].str());
        m_port = static_cast<uint16_t>(std::stoi(ipv4Match[2].str()));
        logger->debug("Parsed IPv4 endpoint: {}", endpointStr);
        return;
    }
    
    // Parse IPv6 endpoint (e.g., "[2001:db8::1]:8080")
    std::regex ipv6Regex(R"(^\[([^\]]+)\]:(\d+)$)");
    std::smatch ipv6Match;
    if (std::regex_match(endpointStr, ipv6Match, ipv6Regex)) {
        m_address = NetworkAddress(ipv6Match[1].str());
        m_port = static_cast<uint16_t>(std::stoi(ipv6Match[2].str()));
        logger->debug("Parsed IPv6 endpoint: {}", endpointStr);
        return;
    }
    
    logger->warning("Failed to parse endpoint: {}", endpointStr);
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
    if (m_address.getFamily() == AddressFamily::IPv6) {
        return "[" + m_address.toString() + "]:" + std::to_string(m_port);
    } else {
        return m_address.toString() + ":" + std::to_string(m_port);
    }
}

bool EndPoint::isValid() const {
    return m_address.isValid() && m_port != 0;
}

bool EndPoint::operator==(const EndPoint& other) const {
    return m_address == other.m_address && m_port == other.m_port;
}

bool EndPoint::operator!=(const EndPoint& other) const {
    return !(*this == other);
}

// AddressResolver implementation

std::vector<NetworkAddress> AddressResolver::resolve(const std::string& hostname, 
                                                   AddressFamily family) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("AddressResolver");
    std::vector<NetworkAddress> addresses;
    
    struct addrinfo hints{};
    struct addrinfo* result = nullptr;
    
    std::memset(&hints, 0, sizeof(hints));
    
    if (family == AddressFamily::IPv4) {
        hints.ai_family = AF_INET;
    } else if (family == AddressFamily::IPv6) {
        hints.ai_family = AF_INET6;
    } else {
        hints.ai_family = AF_UNSPEC;
    }
    
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    
    int status = getaddrinfo(hostname.c_str(), nullptr, &hints, &result);
    if (status != 0) {
        logger->error("Failed to resolve hostname: {}, error: {}", hostname, gai_strerror(status));
        return addresses;
    }
    
    for (struct addrinfo* p = result; p != nullptr; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            struct sockaddr_in* ipv4 = reinterpret_cast<struct sockaddr_in*>(p->ai_addr);
            addresses.emplace_back(ntohl(ipv4->sin_addr.s_addr));
        } else if (p->ai_family == AF_INET6) {
            struct sockaddr_in6* ipv6 = reinterpret_cast<struct sockaddr_in6*>(p->ai_addr);
            std::array<uint8_t, 16> addr{};
            std::memcpy(addr.data(), ipv6->sin6_addr.s6_addr, 16);
            addresses.emplace_back(addr);
        }
    }
    
    freeaddrinfo(result);
    
    logger->debug("Resolved {} to {} addresses", hostname, addresses.size());
    return addresses;
}

std::vector<EndPoint> AddressResolver::resolveEndpoint(const std::string& hostname, 
                                                     uint16_t port,
                                                     AddressFamily family) {
    std::vector<EndPoint> endpoints;
    std::vector<NetworkAddress> addresses = resolve(hostname, family);
    
    for (const auto& address : addresses) {
        endpoints.emplace_back(address, port);
    }
    
    return endpoints;
}

} // namespace dht_hunter::network
