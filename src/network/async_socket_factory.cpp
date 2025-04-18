#include "dht_hunter/network/async_socket_factory.hpp"

namespace dht_hunter::network {

AsyncSocketFactory::AsyncSocketFactory(std::shared_ptr<IOMultiplexer> multiplexer)
    : m_multiplexer(multiplexer) {
}

std::unique_ptr<AsyncSocket> AsyncSocketFactory::createTCPSocket(AddressFamily family) {
    auto socket = SocketFactory::createTCPSocket(family);
    return std::make_unique<AsyncSocket>(std::move(socket), m_multiplexer);
}

std::unique_ptr<AsyncSocket> AsyncSocketFactory::createUDPSocket(AddressFamily family) {
    auto socket = SocketFactory::createUDPSocket(family);
    return std::make_unique<AsyncSocket>(std::move(socket), m_multiplexer);
}

std::shared_ptr<IOMultiplexer> AsyncSocketFactory::getMultiplexer() const {
    return m_multiplexer;
}

} // namespace dht_hunter::network
