#include "dht_hunter/network/socket/socket.hpp"
#include "dht_hunter/network/socket/udp_socket.hpp"
#include <memory>

namespace dht_hunter::network {

std::shared_ptr<Socket> Socket::create(SocketType type, AddressFamily family) {
    if (type == SocketType::UDP) {
        return UDPSocket::create(family);
    } else {
        // TCP socket not implemented yet
        return nullptr;
    }
}

} // namespace dht_hunter::network
