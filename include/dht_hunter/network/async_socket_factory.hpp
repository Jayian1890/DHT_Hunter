#pragma once

#include "async_socket.hpp"
#include "socket_factory.hpp"
#include "io_multiplexer.hpp"
#include "socket.hpp"
#include "network_address.hpp"
#include <memory>

namespace dht_hunter::network {

/**
 * @class AsyncSocketFactory
 * @brief Factory for creating asynchronous sockets.
 */
class AsyncSocketFactory {
public:
    /**
     * @brief Constructor.
     * @param multiplexer The I/O multiplexer to use.
     */
    explicit AsyncSocketFactory(std::shared_ptr<IOMultiplexer> multiplexer);

    /**
     * @brief Creates a TCP socket.
     * @param family The address family to use.
     * @return The created socket.
     */
    std::unique_ptr<AsyncSocket> createTCPSocket(AddressFamily family = AddressFamily::IPv4);

    /**
     * @brief Creates a UDP socket.
     * @param family The address family to use.
     * @return The created socket.
     */
    std::unique_ptr<AsyncSocket> createUDPSocket(AddressFamily family = AddressFamily::IPv4);

    /**
     * @brief Gets the I/O multiplexer.
     * @return The I/O multiplexer.
     */
    std::shared_ptr<IOMultiplexer> getMultiplexer() const;

private:
    std::shared_ptr<IOMultiplexer> m_multiplexer;  ///< The I/O multiplexer
};

} // namespace dht_hunter::network
