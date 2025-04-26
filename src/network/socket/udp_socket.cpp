#include "dht_hunter/network/socket/udp_socket.hpp"
#include "dht_hunter/network/platform/unix/socket_impl.hpp"
#include <memory>

namespace dht_hunter::network {

std::shared_ptr<UDPSocket> UDPSocket::create(AddressFamily family) {
    return std::make_shared<UDPSocketImpl>(family);
}

} // namespace dht_hunter::network
