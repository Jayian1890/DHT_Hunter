#include "dht_hunter/network/io_multiplexer.hpp"

#ifdef _WIN32
#include "dht_hunter/network/platform/windows/wsa_multiplexer.hpp"
#else
#include "dht_hunter/network/platform/unix/select_multiplexer.hpp"
#endif

namespace dht_hunter::network {

std::unique_ptr<IOMultiplexer> IOMultiplexer::create() {
#ifdef _WIN32
    return std::make_unique<WSAMultiplexer>();
#else
    return std::make_unique<SelectMultiplexer>();
#endif
}

} // namespace dht_hunter::network
