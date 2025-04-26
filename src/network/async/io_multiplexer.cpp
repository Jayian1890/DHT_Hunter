#include "dht_hunter/network/async/io_multiplexer.hpp"
#include "dht_hunter/network/platform/unix/select_multiplexer.hpp"
#include "dht_hunter/network/platform/windows/wsa_multiplexer.hpp"

namespace dht_hunter::network {

std::shared_ptr<IOMultiplexer> IOMultiplexer::create() {
#ifdef _WIN32
    return std::make_shared<WSAMultiplexer>();
#else
    return std::make_shared<SelectMultiplexer>();
#endif
}

} // namespace dht_hunter::network
