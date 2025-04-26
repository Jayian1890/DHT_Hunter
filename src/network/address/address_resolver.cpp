#include "dht_hunter/network/address/address_resolver.hpp"
#include <netdb.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <cstring>
#include <vector>

namespace dht_hunter::network {

std::string AddressResolver::getLocalHostname() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return "";
    }
    hostname[sizeof(hostname) - 1] = '\0'; // Ensure null-termination
    return std::string(hostname);
}

std::vector<NetworkAddress> AddressResolver::getLocalAddresses() {
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
        } else if (ifa->ifa_addr->sa_family == AF_INET6) {
            // IPv6 address
            struct sockaddr_in6* addr = reinterpret_cast<struct sockaddr_in6*>(ifa->ifa_addr);
            std::array<uint8_t, 16> ipv6;
            std::memcpy(ipv6.data(), &addr->sin6_addr, 16);
            addresses.push_back(NetworkAddress(ipv6));
        }
    }

    if (ifAddrStruct != nullptr) {
        freeifaddrs(ifAddrStruct);
    }

    return addresses;
}

} // namespace dht_hunter::network
