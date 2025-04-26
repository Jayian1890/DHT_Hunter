#pragma once

#include "dht_hunter/network/address/address_resolver.hpp"
#include "dht_hunter/network/address/network_address.hpp"
#include "dht_hunter/network/address/endpoint.hpp"
#include <string>
#include <vector>

namespace dht_hunter::network {

/**
 * @class IPv4Resolver
 * @brief Resolves hostnames to IPv4 addresses.
 *
 * This class provides methods for resolving hostnames to IPv4 addresses,
 * with support for caching and fallback mechanisms.
 */
class IPv4Resolver : public AddressResolver {
public:
    /**
     * @brief Default constructor.
     */
    IPv4Resolver() = default;

    /**
     * @brief Virtual destructor.
     */
    virtual ~IPv4Resolver() = default;

    /**
     * @brief Resolves a hostname to a list of IPv4 addresses.
     * @param hostname The hostname to resolve.
     * @return A list of IPv4 addresses.
     */
    std::vector<NetworkAddress> resolve(const std::string& hostname) override;

    /**
     * @brief Resolves a hostname and port to a list of IPv4 endpoints.
     * @param hostname The hostname to resolve.
     * @param port The port number.
     * @return A list of IPv4 endpoints.
     */
    std::vector<EndPoint> resolveEndpoint(const std::string& hostname, uint16_t port) override;

    /**
     * @brief Static method to resolve a hostname to a list of IPv4 addresses.
     * @param hostname The hostname to resolve.
     * @return A list of IPv4 addresses.
     */
    static std::vector<NetworkAddress> resolveStatic(const std::string& hostname);

    /**
     * @brief Static method to resolve a hostname and port to a list of IPv4 endpoints.
     * @param hostname The hostname to resolve.
     * @param port The port number.
     * @return A list of IPv4 endpoints.
     */
    static std::vector<EndPoint> resolveEndpoint(const std::string& hostname, uint16_t port);

    /**
     * @brief Gets the local IPv4 addresses.
     * @return A list of local IPv4 addresses.
     */
    static std::vector<NetworkAddress> getLocalIPv4Addresses();
};

} // namespace dht_hunter::network
