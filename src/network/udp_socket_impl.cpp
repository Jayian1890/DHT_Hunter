#include "dht_hunter/network/platform/socket_impl.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"

DEFINE_COMPONENT_LOGGER("Network", "UDPSocketImpl")

namespace dht_hunter::network {
// UDPSocketImpl implementation
UDPSocketImpl::UDPSocketImpl(bool ipv6)
    : SocketImpl(SocketType::UDP, ipv6) {
    getLogger()->trace("UDPSocketImpl constructor start");

    // UDP-specific initialization
    if (SocketImpl::isValid()) {
        // Set socket buffer sizes for better performance
        const int recvBufSize = 262144;  // 256 KB
        const int sendBufSize = 262144;  // 256 KB

        if (setsockopt(SocketImpl::getHandle(), SOL_SOCKET, SO_RCVBUF,
                       reinterpret_cast<const char*>(&recvBufSize), sizeof(recvBufSize)) != 0) {
            getLogger()->warning("Failed to set UDP receive buffer size: {}",
                          Socket::getErrorString(translateError(getLastErrorCode())));
        }

        if (setsockopt(SocketImpl::getHandle(), SOL_SOCKET, SO_SNDBUF,
                       reinterpret_cast<const char*>(&sendBufSize), sizeof(sendBufSize)) != 0) {
            getLogger()->warning("Failed to set UDP send buffer size: {}",
                          Socket::getErrorString(translateError(getLastErrorCode())));
        }

        // Set reuse address option to allow binding to recently used ports
        int reuseAddr = 1;
        if (setsockopt(SocketImpl::getHandle(), SOL_SOCKET, SO_REUSEADDR,
                       reinterpret_cast<const char*>(&reuseAddr), sizeof(reuseAddr)) != 0) {
            getLogger()->warning("Failed to set SO_REUSEADDR: {}",
                          Socket::getErrorString(translateError(getLastErrorCode())));
        }
    }

    getLogger()->debug("UDPSocketImpl constructor completed (IPv{})",
                      ipv6 ? "6" : "4");
}
UDPSocketImpl::UDPSocketImpl(SocketHandle handle)
    : SocketImpl(handle, SocketType::UDP) {
    getLogger()->trace("UDPSocketImpl from handle constructor start");

    // For existing handles, we don't modify socket options as they should be
    // already configured by the creator of the socket

    getLogger()->debug("UDPSocketImpl constructor from handle {} completed",
                      static_cast<int>(handle));
}
int UDPSocketImpl::sendTo(const uint8_t* data, size_t length, const EndPoint& destination) {
    if (!isValid()) {
        getLogger()->error("Cannot send to on invalid socket");
        return -1;
    }
    ssize_t result = -1;
    if (destination.getAddress().getFamily() == AddressFamily::IPv6) {
        struct sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(destination.getPort());
        std::memcpy(addr.sin6_addr.s6_addr,
                   destination.getAddress().getIPv6Address().data(), 16);
        result = ::sendto(getHandle(), reinterpret_cast<const char*>(data),
                         length, 0,
                         reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    } else {
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(destination.getPort());
        addr.sin_addr.s_addr = htonl(destination.getAddress().getIPv4Address());
        result = ::sendto(getHandle(), reinterpret_cast<const char*>(data),
                         length, 0,
                         reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    }
    if (result < 0) {
        getLogger()->error("Failed to send data to {}: {}",
                     destination.toString(),
                     Socket::getErrorString(translateError(getLastErrorCode())));
        return -1;
    }
    getLogger()->trace("Sent {} bytes to {}", result, destination.toString());
    return static_cast<int>(result);
}
int UDPSocketImpl::receiveFrom(uint8_t* buffer, size_t maxLength, EndPoint& source) {
    if (!isValid()) {
        getLogger()->error("Cannot receive from on invalid socket");
        return -1;
    }
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);
    ssize_t result = ::recvfrom(getHandle(), reinterpret_cast<char*>(buffer),
                              maxLength, 0,
                              reinterpret_cast<struct sockaddr*>(&addr), &addrLen);
    if (result < 0) {
        getLogger()->error("Failed to receive data: {}",
                     Socket::getErrorString(translateError(getLastErrorCode())));
        return -1;
    }
    // Extract source address and port
    if (addr.ss_family == AF_INET) {
        struct sockaddr_in* ipv4 = reinterpret_cast<struct sockaddr_in*>(&addr);
        source = EndPoint(NetworkAddress(ntohl(ipv4->sin_addr.s_addr)), ntohs(ipv4->sin_port));
    } else if (addr.ss_family == AF_INET6) {
        struct sockaddr_in6* ipv6 = reinterpret_cast<struct sockaddr_in6*>(&addr);
        std::array<uint8_t, 16> ipv6Addr{};
        std::memcpy(ipv6Addr.data(), ipv6->sin6_addr.s6_addr, 16);
        source = EndPoint(NetworkAddress(ipv6Addr), ntohs(ipv6->sin6_port));
    }
    getLogger()->trace("Received {} bytes from {}", result, source.toString());
    return static_cast<int>(result);
}
bool UDPSocketImpl::setBroadcast(bool broadcast) {
    if (!isValid()) {
        getLogger()->error("Cannot set broadcast on invalid socket");
        return false;
    }
    int value = broadcast ? 1 : 0;
    if (setsockopt(getHandle(), SOL_SOCKET, SO_BROADCAST,
                  reinterpret_cast<char*>(&value), sizeof(value)) != 0) {
        getLogger()->error("Failed to set SO_BROADCAST: {}",
                     Socket::getErrorString(translateError(getLastErrorCode())));
        return false;
    }
    getLogger()->debug("Socket broadcast {}", broadcast ? "enabled" : "disabled");
    return true;
}
bool UDPSocketImpl::joinMulticastGroup(const NetworkAddress& groupAddress,
                                     const NetworkAddress& interfaceAddress) {
    if (!isValid()) {
        getLogger()->error("Cannot join multicast group on invalid socket");
        return false;
    }
    if (!groupAddress.isMulticast()) {
        getLogger()->error("Cannot join non-multicast address: {}", groupAddress.toString());
        return false;
    }
    if (groupAddress.getFamily() == AddressFamily::IPv6) {
        struct ipv6_mreq mreq{};
        std::memcpy(mreq.ipv6mr_multiaddr.s6_addr,
                   groupAddress.getIPv6Address().data(), 16);
        // If interface address is specified, find its index
        if (interfaceAddress.isValid() && interfaceAddress.getFamily() == AddressFamily::IPv6) {
            // This is simplified; in a real implementation, you would need to find the interface index
            mreq.ipv6mr_interface = 0;  // Use default interface
        } else {
            mreq.ipv6mr_interface = 0;  // Use default interface
        }
        if (setsockopt(getHandle(), IPPROTO_IPV6, IPV6_JOIN_GROUP,
                      reinterpret_cast<char*>(&mreq), sizeof(mreq)) != 0) {
            getLogger()->error("Failed to join IPv6 multicast group {}: {}",
                         groupAddress.toString(),
                         Socket::getErrorString(translateError(getLastErrorCode())));
            return false;
        }
    } else {
        struct ip_mreq mreq{};
        mreq.imr_multiaddr.s_addr = htonl(groupAddress.getIPv4Address());
        if (interfaceAddress.isValid() && interfaceAddress.getFamily() == AddressFamily::IPv4) {
            mreq.imr_interface.s_addr = htonl(interfaceAddress.getIPv4Address());
        } else {
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        }
        if (setsockopt(getHandle(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
                      reinterpret_cast<char*>(&mreq), sizeof(mreq)) != 0) {
            getLogger()->error("Failed to join IPv4 multicast group {}: {}",
                         groupAddress.toString(),
                         Socket::getErrorString(translateError(getLastErrorCode())));
            return false;
        }
    }
    getLogger()->debug("Joined multicast group {}", groupAddress.toString());
    return true;
}
std::unique_ptr<Socket> UDPSocketImpl::accept(EndPoint& /* endpoint */) {
    getLogger()->error("Cannot accept on UDP socket");
    return nullptr;
}
bool UDPSocketImpl::leaveMulticastGroup(const NetworkAddress& groupAddress,
                                      const NetworkAddress& interfaceAddress) {
    if (!isValid()) {
        getLogger()->error("Cannot leave multicast group on invalid socket");
        return false;
    }
    if (!groupAddress.isMulticast()) {
        getLogger()->error("Cannot leave non-multicast address: {}", groupAddress.toString());
        return false;
    }
    if (groupAddress.getFamily() == AddressFamily::IPv6) {
        struct ipv6_mreq mreq{};
        std::memcpy(mreq.ipv6mr_multiaddr.s6_addr,
                   groupAddress.getIPv6Address().data(), 16);
        // If interface address is specified, find its index
        if (interfaceAddress.isValid() && interfaceAddress.getFamily() == AddressFamily::IPv6) {
            // This is simplified; in a real implementation, you would need to find the interface index
            mreq.ipv6mr_interface = 0;  // Use default interface
        } else {
            mreq.ipv6mr_interface = 0;  // Use default interface
        }
        if (setsockopt(getHandle(), IPPROTO_IPV6, IPV6_LEAVE_GROUP,
                      reinterpret_cast<char*>(&mreq), sizeof(mreq)) != 0) {
            getLogger()->error("Failed to leave IPv6 multicast group {}: {}",
                         groupAddress.toString(),
                         Socket::getErrorString(translateError(getLastErrorCode())));
            return false;
        }
    } else {
        struct ip_mreq mreq{};
        mreq.imr_multiaddr.s_addr = htonl(groupAddress.getIPv4Address());
        if (interfaceAddress.isValid() && interfaceAddress.getFamily() == AddressFamily::IPv4) {
            mreq.imr_interface.s_addr = htonl(interfaceAddress.getIPv4Address());
        } else {
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        }
        if (setsockopt(getHandle(), IPPROTO_IP, IP_DROP_MEMBERSHIP,
                      reinterpret_cast<char*>(&mreq), sizeof(mreq)) != 0) {
            getLogger()->error("Failed to leave IPv4 multicast group {}: {}",
                         groupAddress.toString(),
                         Socket::getErrorString(translateError(getLastErrorCode())));
            return false;
        }
    }
    getLogger()->debug("Left multicast group {}", groupAddress.toString());
    return true;
}
} // namespace dht_hunter::network
