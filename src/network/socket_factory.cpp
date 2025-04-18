#include "dht_hunter/network/socket_factory.hpp"
#include "dht_hunter/network/platform/socket_impl.hpp"
#include "dht_hunter/logforge/logforge.hpp"

namespace dht_hunter::network {

std::unique_ptr<Socket> SocketFactory::createTCPSocket(bool ipv6) {
    // No logging for now to avoid test hanging
    return std::make_unique<TCPSocketImpl>(ipv6);
}

std::unique_ptr<Socket> SocketFactory::createUDPSocket(bool ipv6) {
    // No logging for now to avoid test hanging
    return std::make_unique<UDPSocketImpl>(ipv6);
}

} // namespace dht_hunter::network
