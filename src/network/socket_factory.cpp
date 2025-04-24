#include "dht_hunter/network/socket_factory.hpp"
#include "dht_hunter/network/platform/socket_impl.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"

DEFINE_COMPONENT_LOGGER("Network", "SocketFactory")

namespace dht_hunter::network {

std::unique_ptr<Socket> SocketFactory::createTCPSocket(bool ipv6) {
    getLogger()->trace("Creating TCP socket (IPv{}) - start", ipv6 ? "6" : "4");
    
    auto socket = std::make_unique<TCPSocketImpl>(ipv6);
    
    getLogger()->debug("Created TCP socket (IPv{}) successfully", ipv6 ? "6" : "4");
    return socket;
}

std::unique_ptr<Socket> SocketFactory::createUDPSocket(bool ipv6) {
    getLogger()->trace("Creating UDP socket (IPv{}) - start", ipv6 ? "6" : "4");
    
    auto socket = std::make_unique<UDPSocketImpl>(ipv6);
    
    getLogger()->debug("Created UDP socket (IPv{}) successfully", ipv6 ? "6" : "4");
    return socket;
}

} // namespace dht_hunter::network
