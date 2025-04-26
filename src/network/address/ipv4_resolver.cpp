#include "dht_hunter/network/address/ipv4_resolver.hpp"
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <cstring>
#include <vector>

namespace dht_hunter::network {

std::vector<NetworkAddress> IPv4Resolver::resolve(const std::string& hostname) {
    return resolveStatic(hostname);
}

std::vector<EndPoint> IPv4Resolver::resolveEndpoint(const std::string& hostname, uint16_t port) {
    return resolveEndpoint(hostname, port);
}

std::vector<NetworkAddress> IPv4Resolver::resolveStatic(const std::string& hostname) {
    std::vector<NetworkAddress> addresses;

    // Check if the hostname is already an IPv4 address
    struct in_addr addr;
    if (inet_pton(AF_INET, hostname.c_str(), &addr) == 1) {
        addresses.push_back(NetworkAddress(ntohl(addr.s_addr)));
        return addresses;
    }

    // Resolve the hostname
    struct addrinfo hints;
    struct addrinfo* result;
    struct addrinfo* res;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4 only
    hints.ai_socktype = SOCK_DGRAM; // UDP socket

    int status = getaddrinfo(hostname.c_str(), nullptr, &hints, &result);
    if (status != 0) {
        return addresses;
    }

    for (res = result; res != nullptr; res = res->ai_next) {
        if (res->ai_family == AF_INET) {
            struct sockaddr_in* ipv4 = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
            uint32_t ipv4Addr = ntohl(ipv4->sin_addr.s_addr);
            addresses.push_back(NetworkAddress(ipv4Addr));
        }
    }

    freeaddrinfo(result);
    return addresses;
}

std::vector<EndPoint> IPv4Resolver::resolveEndpoint(const std::string& hostname, uint16_t port) {
    std::vector<EndPoint> endpoints;
    std::vector<NetworkAddress> addresses = resolveStatic(hostname);

    for (const auto& address : addresses) {
        endpoints.push_back(EndPoint(address, port));
    }

    return endpoints;
}

std::vector<NetworkAddress> IPv4Resolver::getLocalIPv4Addresses() {
    std::vector<NetworkAddress> addresses;
    struct ifaddrs* ifAddrStruct = nullptr;
    struct ifaddrs* ifa = nullptr;

    if (getifaddrs(&ifAddrStruct) == -1) {
        return addresses;
    }

    for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) {
            continue;
        }

        // Skip loopback interfaces
        if (ifa->ifa_flags & IFF_LOOPBACK) {
            continue;
        }

        // Skip interfaces that are not up
        if (!(ifa->ifa_flags & IFF_UP)) {
            continue;
        }

        if (ifa->ifa_addr->sa_family == AF_INET) {
            // IPv4 address
            struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
            uint32_t ipv4 = ntohl(addr->sin_addr.s_addr);
            addresses.push_back(NetworkAddress(ipv4));
        }
    }

    if (ifAddrStruct != nullptr) {
        freeifaddrs(ifAddrStruct);
    }

    return addresses;
}

} // namespace dht_hunter::network
