#pragma once

#include "dht_hunter/network/address/network_address.hpp"
#include "dht_hunter/network/address/endpoint.hpp"
#include <functional>
#include <cstddef>

namespace std {

/**
 * @brief Hash specialization for dht_hunter::network::NetworkAddress
 */
template<>
struct hash<dht_hunter::network::NetworkAddress> {
    /**
     * @brief Computes a hash value for a NetworkAddress
     * @param address The network address
     * @return The hash value
     */
    size_t operator()(const dht_hunter::network::NetworkAddress& address) const {
        if (address.getFamily() == dht_hunter::network::AddressFamily::IPv4) {
            // For IPv4, use the address directly as the hash
            return static_cast<size_t>(address.getIPv4Address());
        } else if (address.getFamily() == dht_hunter::network::AddressFamily::IPv6) {
            // For IPv6, compute a hash from the 16-byte array
            const auto& ipv6 = address.getIPv6Address();

            // Use FNV-1a hash algorithm
            size_t hash = 14695981039346656037ULL; // FNV offset basis
            for (size_t i = 0; i < ipv6.size(); ++i) {
                hash ^= static_cast<size_t>(ipv6[i]);
                hash *= 1099511628211ULL; // FNV prime
            }
            return hash;
        } else {
            // For unspecified addresses, use a constant hash
            return 0;
        }
    }

    // Required for C++11 hash requirements
    typedef dht_hunter::network::NetworkAddress argument_type;
    typedef std::size_t result_type;
};

/**
 * @brief Hash specialization for dht_hunter::network::EndPoint
 */
template<>
struct hash<dht_hunter::network::EndPoint> {
    /**
     * @brief Computes a hash value for an EndPoint
     * @param endpoint The network endpoint
     * @return The hash value
     */
    size_t operator()(const dht_hunter::network::EndPoint& endpoint) const {
        // Combine the address hash with the port
        size_t addressHash = std::hash<dht_hunter::network::NetworkAddress>{}(endpoint.getAddress());
        return addressHash ^ (static_cast<size_t>(endpoint.getPort()) + 0x9e3779b9 + (addressHash << 6) + (addressHash >> 2));
    }

    // Required for C++11 hash requirements
    typedef dht_hunter::network::EndPoint argument_type;
    typedef std::size_t result_type;
};

} // namespace std
