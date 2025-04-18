#include "dht_hunter/network/platform/socket_impl.hpp"
#include "dht_hunter/logforge/logforge.hpp"

namespace dht_hunter::network {

// TCPSocketImpl implementation

TCPSocketImpl::TCPSocketImpl(bool ipv6)
    : SocketImpl(SocketType::TCP, ipv6) {
}

TCPSocketImpl::TCPSocketImpl(SocketHandle handle)
    : SocketImpl(handle, SocketType::TCP) {
}

bool TCPSocketImpl::listen(int backlog) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("TCPSocketImpl");

    if (!isValid()) {
        logger->error("Cannot listen on invalid socket");
        return false;
    }

    if (::listen(getHandle(), backlog) != 0) {
        logger->error("Failed to listen on socket: {}",
                     Socket::getErrorString(translateError(getLastErrorCode())));
        return false;
    }

    logger->debug("Socket listening with backlog {}", backlog);
    return true;
}

std::unique_ptr<TCPSocketImpl> TCPSocketImpl::accept() {
    auto logger = dht_hunter::logforge::LogForge::getLogger("TCPSocketImpl");

    if (!isValid()) {
        logger->error("Cannot accept on invalid socket");
        return nullptr;
    }

    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);

    SocketHandle clientHandle = ::accept(getHandle(),
                                       reinterpret_cast<struct sockaddr*>(&addr),
                                       &addrLen);

    if (clientHandle == INVALID_SOCKET_HANDLE) {
        logger->error("Failed to accept connection: {}",
                     Socket::getErrorString(translateError(getLastErrorCode())));
        return nullptr;
    }

    // Create a new socket from the accepted handle
    auto clientSocket = std::make_unique<TCPSocketImpl>(clientHandle);

    // Log the client address
    char ipStr[INET6_ADDRSTRLEN];
    uint16_t port = 0;

    if (addr.ss_family == AF_INET) {
        struct sockaddr_in* ipv4 = reinterpret_cast<struct sockaddr_in*>(&addr);
        inet_ntop(AF_INET, &ipv4->sin_addr, ipStr, INET_ADDRSTRLEN);
        port = ntohs(ipv4->sin_port);
    } else if (addr.ss_family == AF_INET6) {
        struct sockaddr_in6* ipv6 = reinterpret_cast<struct sockaddr_in6*>(&addr);
        inet_ntop(AF_INET6, &ipv6->sin6_addr, ipStr, INET6_ADDRSTRLEN);
        port = ntohs(ipv6->sin6_port);
    }

    logger->debug("Accepted connection from {}:{}", ipStr, port);
    return clientSocket;
}

std::unique_ptr<Socket> TCPSocketImpl::accept(EndPoint& endpoint) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("TCPSocketImpl");

    if (!isValid()) {
        logger->error("Cannot accept on invalid socket");
        return nullptr;
    }

    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);

    SocketHandle clientHandle = ::accept(getHandle(),
                                       reinterpret_cast<struct sockaddr*>(&addr),
                                       &addrLen);

    if (clientHandle == INVALID_SOCKET_HANDLE) {
        logger->error("Failed to accept connection: {}",
                     Socket::getErrorString(translateError(getLastErrorCode())));
        return nullptr;
    }

    // Create a new socket from the accepted handle
    auto clientSocket = std::make_unique<TCPSocketImpl>(clientHandle);

    // Convert the sockaddr to an EndPoint
    char ipStr[INET6_ADDRSTRLEN];
    uint16_t port = 0;

    if (addr.ss_family == AF_INET) {
        struct sockaddr_in* ipv4 = reinterpret_cast<struct sockaddr_in*>(&addr);
        inet_ntop(AF_INET, &ipv4->sin_addr, ipStr, INET_ADDRSTRLEN);
        port = ntohs(ipv4->sin_port);
        endpoint = EndPoint(NetworkAddress(ntohl(ipv4->sin_addr.s_addr)), port);
    } else if (addr.ss_family == AF_INET6) {
        struct sockaddr_in6* ipv6 = reinterpret_cast<struct sockaddr_in6*>(&addr);
        inet_ntop(AF_INET6, &ipv6->sin6_addr, ipStr, INET6_ADDRSTRLEN);
        port = ntohs(ipv6->sin6_port);
        std::array<uint8_t, 16> ipv6Addr;
        std::memcpy(ipv6Addr.data(), &ipv6->sin6_addr, 16);
        endpoint = EndPoint(NetworkAddress(ipv6Addr), port);
    }

    logger->debug("Accepted connection from {}:{}", ipStr, port);
    return clientSocket;
}

bool TCPSocketImpl::setNoDelay(bool noDelay) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("TCPSocketImpl");

    if (!isValid()) {
        logger->error("Cannot set no delay on invalid socket");
        return false;
    }

    int value = noDelay ? 1 : 0;
    if (setsockopt(getHandle(), IPPROTO_TCP, TCP_NODELAY,
                  reinterpret_cast<char*>(&value), sizeof(value)) != 0) {
        logger->error("Failed to set TCP_NODELAY: {}",
                     Socket::getErrorString(translateError(getLastErrorCode())));
        return false;
    }

    logger->debug("Socket set to {} Nagle's algorithm", noDelay ? "disable" : "enable");
    return true;
}

} // namespace dht_hunter::network
