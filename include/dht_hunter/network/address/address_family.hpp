#pragma once

namespace dht_hunter::network {

/**
 * @enum AddressFamily
 * @brief Defines the address family (IPv4 or IPv6).
 */
enum class AddressFamily {
    IPv4,
    IPv6,
    Unspecified
};

} // namespace dht_hunter::network
