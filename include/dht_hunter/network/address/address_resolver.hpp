#pragma once

#include "dht_hunter/network/address/network_address.hpp"
#include "dht_hunter/network/address/endpoint.hpp"
#include <string>
#include <vector>

namespace dht_hunter::network {

/**
 * @class AddressResolver
 * @brief Base class for resolving hostnames to IP addresses.
 *
 * This class provides an interface for resolving hostnames to IP addresses,
 * with support for both IPv4 and IPv6.
 */
class AddressResolver {
public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~AddressResolver() = default;

    /**
     * @brief Resolves a hostname to a list of IP addresses.
     * @param hostname The hostname to resolve.
     * @return A list of IP addresses.
     */
    virtual std::vector<NetworkAddress> resolve(const std::string& hostname) = 0;

    /**
     * @brief Resolves a hostname and port to a list of endpoints.
     * @param hostname The hostname to resolve.
     * @param port The port number.
     * @return A list of endpoints.
     */
    virtual std::vector<EndPoint> resolveEndpoint(const std::string& hostname, uint16_t port) = 0;

    /**
     * @brief Gets the local hostname.
     * @return The local hostname.
     */
    static std::string getLocalHostname();

    /**
     * @brief Gets the local IP addresses.
     * @return A list of local IP addresses.
     */
    static std::vector<NetworkAddress> getLocalAddresses();
};

} // namespace dht_hunter::network
