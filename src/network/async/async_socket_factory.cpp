#include "dht_hunter/network/async/async_socket_factory.hpp"
#include "dht_hunter/network/socket/socket.hpp"

namespace dht_hunter::network {

AsyncSocketFactory::AsyncSocketFactory(std::shared_ptr<IOMultiplexer> multiplexer)
    : m_multiplexer(multiplexer) {
    
    if (!m_multiplexer) {
        m_multiplexer = IOMultiplexer::create();
    }
}

AsyncSocketFactory::~AsyncSocketFactory() {
}

std::shared_ptr<AsyncSocket> AsyncSocketFactory::createTCPSocket(AddressFamily family) {
    auto socket = Socket::create(SocketType::TCP, family);
    if (!socket) {
        return nullptr;
    }
    
    return createFromSocket(socket);
}

std::shared_ptr<AsyncUDPSocket> AsyncSocketFactory::createUDPSocket(AddressFamily family) {
    auto socket = Socket::create(SocketType::UDP, family);
    if (!socket) {
        return nullptr;
    }
    
    return std::dynamic_pointer_cast<AsyncUDPSocket>(createFromSocket(socket));
}

std::shared_ptr<AsyncSocket> AsyncSocketFactory::createFromSocket(std::shared_ptr<Socket> socket) {
    if (!socket) {
        return nullptr;
    }
    
    return AsyncSocket::create(socket, m_multiplexer);
}

std::shared_ptr<IOMultiplexer> AsyncSocketFactory::getMultiplexer() const {
    return m_multiplexer;
}

void AsyncSocketFactory::setMultiplexer(std::shared_ptr<IOMultiplexer> multiplexer) {
    if (multiplexer) {
        m_multiplexer = multiplexer;
    }
}

AsyncSocketFactory& AsyncSocketFactory::getInstance() {
    static AsyncSocketFactory instance;
    return instance;
}

} // namespace dht_hunter::network
