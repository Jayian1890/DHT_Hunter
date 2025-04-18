#include "dht_hunter/network/socket_factory.hpp"
#include "dht_hunter/network/platform/socket_impl.hpp"
#include "dht_hunter/logforge/logforge.hpp"

namespace dht_hunter::network {

std::unique_ptr<Socket> SocketFactory::createTCPSocket(bool ipv6) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("SocketFactory");
    logger->debug("Creating TCP socket (IPv{})", ipv6 ? "6" : "4");
    return std::make_unique<TCPSocketImpl>(ipv6);
}

std::unique_ptr<Socket> SocketFactory::createUDPSocket(bool ipv6) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("SocketFactory");
    logger->debug("Creating UDP socket (IPv{})", ipv6 ? "6" : "4");
    return std::make_unique<UDPSocketImpl>(ipv6);
}

} // namespace dht_hunter::network
