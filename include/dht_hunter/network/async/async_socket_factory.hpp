#pragma once

#include "dht_hunter/network/address/address_family.hpp"
#include "dht_hunter/network/async/async_socket.hpp"
#include "dht_hunter/network/async/io_multiplexer.hpp"
#include "dht_hunter/network/socket/socket_type.hpp"
#include <memory>

namespace dht_hunter::network {

/**
 * @class AsyncSocketFactory
 * @brief Factory for creating asynchronous sockets.
 *
 * This class provides methods for creating asynchronous sockets of different types.
 */
class AsyncSocketFactory {
public:
    /**
     * @brief Constructs an asynchronous socket factory.
     * @param multiplexer The I/O multiplexer to use.
     */
    explicit AsyncSocketFactory(std::shared_ptr<IOMultiplexer> multiplexer = nullptr);

    /**
     * @brief Destructor.
     */
    ~AsyncSocketFactory();

    /**
     * @brief Creates an asynchronous TCP socket.
     * @param family The address family.
     * @return A shared pointer to the created socket.
     */
    std::shared_ptr<AsyncSocket> createTCPSocket(AddressFamily family = AddressFamily::IPv4);

    /**
     * @brief Creates an asynchronous UDP socket.
     * @param family The address family.
     * @return A shared pointer to the created socket.
     */
    std::shared_ptr<AsyncUDPSocket> createUDPSocket(AddressFamily family = AddressFamily::IPv4);

    /**
     * @brief Creates an asynchronous socket from an existing socket.
     * @param socket The existing socket.
     * @return A shared pointer to the created asynchronous socket.
     */
    std::shared_ptr<AsyncSocket> createFromSocket(std::shared_ptr<Socket> socket);

    /**
     * @brief Gets the I/O multiplexer.
     * @return The I/O multiplexer.
     */
    std::shared_ptr<IOMultiplexer> getMultiplexer() const;

    /**
     * @brief Sets the I/O multiplexer.
     * @param multiplexer The I/O multiplexer to use.
     */
    void setMultiplexer(std::shared_ptr<IOMultiplexer> multiplexer);

    /**
     * @brief Gets the singleton instance of the asynchronous socket factory.
     * @return The singleton instance.
     */
    static AsyncSocketFactory& getInstance();

private:
    std::shared_ptr<IOMultiplexer> m_multiplexer;
};

} // namespace dht_hunter::network
