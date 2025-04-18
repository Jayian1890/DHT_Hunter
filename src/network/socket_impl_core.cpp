#include "dht_hunter/network/platform/socket_impl.hpp"
#include "dht_hunter/logforge/logforge.hpp"

namespace dht_hunter::network {

namespace {
    // Get logger for socket implementation
    auto logger = dht_hunter::logforge::LogForge::getLogger("SocketImpl");
}

// SocketImpl implementation

SocketImpl::SocketImpl(SocketType type, bool ipv6)
    : m_handle(INVALID_SOCKET_HANDLE),
      m_type(type),
      m_lastError(SocketError::None),
      m_ipv6(ipv6) {

    int family = ipv6 ? AF_INET6 : AF_INET;
    int sockType = (type == SocketType::TCP) ? SOCK_STREAM : SOCK_DGRAM;
    int protocol = (type == SocketType::TCP) ? IPPROTO_TCP : IPPROTO_UDP;

    m_handle = socket(family, sockType, protocol);

    if (m_handle == INVALID_SOCKET_HANDLE) {
        m_lastError = translateError(getLastErrorCode());
        logger->error("Failed to create socket: {}", getErrorString(m_lastError));
    } else {
        logger->debug("Socket created: {} ({})",
                     static_cast<int>(m_handle),
                     type == SocketType::TCP ? "TCP" : "UDP");

        // Set IPv6 only if it's an IPv6 socket
        if (ipv6) {
            int ipv6Only = 0;  // Allow IPv4 mapped addresses
            if (setsockopt(m_handle, IPPROTO_IPV6, IPV6_V6ONLY,
                          reinterpret_cast<char*>(&ipv6Only), sizeof(ipv6Only)) != 0) {
                logger->warning("Failed to set IPV6_V6ONLY option");
            }
        }
    }
}

SocketImpl::SocketImpl(SocketHandle handle, SocketType type)
    : m_handle(handle),
      m_type(type),
      m_lastError(SocketError::None),
      m_ipv6(false) {

    // Determine if this is an IPv6 socket
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);
    if (getsockname(handle, reinterpret_cast<struct sockaddr*>(&addr), &addrLen) == 0) {
        m_ipv6 = (addr.ss_family == AF_INET6);
    }

    logger->debug("Socket created from handle: {} ({})",
                 static_cast<int>(handle),
                 type == SocketType::TCP ? "TCP" : "UDP");
}

SocketImpl::~SocketImpl() {
    close();
}

// Platform-specific implementations in platform/windows/socket_impl.cpp and platform/unix/socket_impl.cpp
// These are declared here but implemented in platform-specific files
SocketError SocketImpl::translateError(int /* errorCode */) {
    return SocketError::Unknown;
}

int SocketImpl::getLastErrorCode() {
    return 0;
}

void SocketImpl::close() {
    // This is a stub that will be overridden by platform-specific implementations
}

bool SocketImpl::bind(const EndPoint& localEndpoint) {
    if (m_handle == INVALID_SOCKET_HANDLE) {
        m_lastError = SocketError::InvalidSocket;
        logger->error("Cannot bind invalid socket");
        return false;
    }

    if (localEndpoint.getAddress().getFamily() == AddressFamily::IPv6 && !m_ipv6) {
        logger->error("Cannot bind IPv6 address to IPv4 socket");
        m_lastError = SocketError::AddressNotAvailable;
        return false;
    }

    if (localEndpoint.getAddress().getFamily() == AddressFamily::IPv4 && m_ipv6) {
        logger->warning("Binding IPv4 address to IPv6 socket, using IPv4-mapped IPv6 address");
    }

    if (m_ipv6) {
        struct sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(localEndpoint.getPort());

        if (localEndpoint.getAddress().getFamily() == AddressFamily::IPv4) {
            // Create IPv4-mapped IPv6 address
            addr.sin6_addr.s6_addr[10] = 0xFF;
            addr.sin6_addr.s6_addr[11] = 0xFF;
            uint32_t ipv4 = htonl(localEndpoint.getAddress().getIPv4Address());
            std::memcpy(&addr.sin6_addr.s6_addr[12], &ipv4, 4);
        } else {
            // Use IPv6 address
            std::memcpy(addr.sin6_addr.s6_addr,
                       localEndpoint.getAddress().getIPv6Address().data(), 16);
        }

        if (::bind(m_handle, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
            m_lastError = translateError(getLastErrorCode());
            logger->error("Failed to bind socket to [{}]:{}: {}",
                         localEndpoint.getAddress().toString(),
                         localEndpoint.getPort(),
                         getErrorString(m_lastError));
            return false;
        }
    } else {
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(localEndpoint.getPort());
        addr.sin_addr.s_addr = htonl(localEndpoint.getAddress().getIPv4Address());

        if (::bind(m_handle, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
            m_lastError = translateError(getLastErrorCode());
            logger->error("Failed to bind socket to {}:{}: {}",
                         localEndpoint.getAddress().toString(),
                         localEndpoint.getPort(),
                         getErrorString(m_lastError));
            return false;
        }
    }

    logger->debug("Socket bound to {}:{}",
                 localEndpoint.getAddress().toString(),
                 localEndpoint.getPort());
    return true;
}

bool SocketImpl::connect(const EndPoint& remoteEndpoint) {
    if (m_handle == INVALID_SOCKET_HANDLE) {
        m_lastError = SocketError::InvalidSocket;
        logger->error("Cannot connect invalid socket");
        return false;
    }

    if (remoteEndpoint.getAddress().getFamily() == AddressFamily::IPv6 && !m_ipv6) {
        logger->error("Cannot connect to IPv6 address with IPv4 socket");
        m_lastError = SocketError::AddressNotAvailable;
        return false;
    }

    if (remoteEndpoint.getAddress().getFamily() == AddressFamily::IPv4 && m_ipv6) {
        logger->warning("Connecting to IPv4 address with IPv6 socket, using IPv4-mapped IPv6 address");
    }

    if (m_ipv6) {
        struct sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(remoteEndpoint.getPort());

        if (remoteEndpoint.getAddress().getFamily() == AddressFamily::IPv4) {
            // Create IPv4-mapped IPv6 address
            addr.sin6_addr.s6_addr[10] = 0xFF;
            addr.sin6_addr.s6_addr[11] = 0xFF;
            uint32_t ipv4 = htonl(remoteEndpoint.getAddress().getIPv4Address());
            std::memcpy(&addr.sin6_addr.s6_addr[12], &ipv4, 4);
        } else {
            // Use IPv6 address
            std::memcpy(addr.sin6_addr.s6_addr,
                       remoteEndpoint.getAddress().getIPv6Address().data(), 16);
        }

        if (::connect(m_handle, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
            m_lastError = translateError(getLastErrorCode());
            logger->error("Failed to connect socket to [{}]:{}: {}",
                         remoteEndpoint.getAddress().toString(),
                         remoteEndpoint.getPort(),
                         getErrorString(m_lastError));
            return false;
        }
    } else {
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(remoteEndpoint.getPort());
        addr.sin_addr.s_addr = htonl(remoteEndpoint.getAddress().getIPv4Address());

        if (::connect(m_handle, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
            m_lastError = translateError(getLastErrorCode());
            logger->error("Failed to connect socket to {}:{}: {}",
                         remoteEndpoint.getAddress().toString(),
                         remoteEndpoint.getPort(),
                         getErrorString(m_lastError));
            return false;
        }
    }

    logger->debug("Socket connected to {}:{}",
                 remoteEndpoint.getAddress().toString(),
                 remoteEndpoint.getPort());
    return true;
}

int SocketImpl::send(const uint8_t* data, size_t length) {
    if (m_handle == INVALID_SOCKET_HANDLE) {
        m_lastError = SocketError::InvalidSocket;
        logger->error("Cannot send on invalid socket");
        return -1;
    }

    ssize_t result = ::send(m_handle, reinterpret_cast<const char*>(data),
                          length, 0);

    if (result < 0) {
        m_lastError = translateError(getLastErrorCode());
        logger->error("Failed to send data: {}", getErrorString(m_lastError));
        return -1;
    }

    logger->trace("Sent {} bytes", result);
    return static_cast<int>(result);
}

int SocketImpl::receive(uint8_t* buffer, size_t maxLength) {
    if (m_handle == INVALID_SOCKET_HANDLE) {
        m_lastError = SocketError::InvalidSocket;
        logger->error("Cannot receive on invalid socket");
        return -1;
    }

    ssize_t result = ::recv(m_handle, reinterpret_cast<char*>(buffer),
                          maxLength, 0);

    if (result < 0) {
        m_lastError = translateError(getLastErrorCode());
        logger->error("Failed to receive data: {}", getErrorString(m_lastError));
        return -1;
    } else if (result == 0) {
        // Connection closed by peer
        logger->debug("Connection closed by peer");
    } else {
        logger->trace("Received {} bytes", result);
    }

    return static_cast<int>(result);
}

// Platform-specific implementation in platform/windows/socket_impl.cpp and platform/unix/socket_impl.cpp
bool SocketImpl::setNonBlocking(bool /* nonBlocking */) {
    // This is a stub that will be overridden by platform-specific implementations
    return false;
}

bool SocketImpl::setReuseAddress(bool reuse) {
    if (m_handle == INVALID_SOCKET_HANDLE) {
        m_lastError = SocketError::InvalidSocket;
        logger->error("Cannot set reuse address on invalid socket");
        return false;
    }

    int value = reuse ? 1 : 0;
    if (setsockopt(m_handle, SOL_SOCKET, SO_REUSEADDR,
                  reinterpret_cast<char*>(&value), sizeof(value)) != 0) {
        m_lastError = translateError(getLastErrorCode());
        logger->error("Failed to set reuse address: {}", getErrorString(m_lastError));
        return false;
    }

    logger->debug("Socket set to {} address", reuse ? "reuse" : "not reuse");
    return true;
}

} // namespace dht_hunter::network
