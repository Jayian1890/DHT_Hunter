#pragma once

#include "socket.hpp"
#include <memory>

namespace dht_hunter::network {

/**
 * @class SocketFactory
 * @brief Factory for creating platform-specific socket implementations.
 * 
 * This class provides methods for creating TCP and UDP sockets with
 * platform-specific implementations.
 */
class SocketFactory {
public:
    /**
     * @brief Creates a TCP socket.
     * @param ipv6 Whether to create an IPv6 socket.
     * @return A unique pointer to the created socket.
     */
    static std::unique_ptr<Socket> createTCPSocket(bool ipv6 = false);
    
    /**
     * @brief Creates a UDP socket.
     * @param ipv6 Whether to create an IPv6 socket.
     * @return A unique pointer to the created socket.
     */
    static std::unique_ptr<Socket> createUDPSocket(bool ipv6 = false);
};

} // namespace dht_hunter::network
