#include "dht_hunter/network/platform/socket_impl.hpp"
#include "dht_hunter/logforge/logforge.hpp"

namespace dht_hunter::network {

namespace {
    // Get logger for socket implementation
    auto logger = dht_hunter::logforge::LogForge::getLogger("SocketImpl");
}

// Socket error string implementation is in socket.cpp

// Common SocketImpl methods that don't depend on platform specifics

SocketType SocketImpl::getType() const {
    return m_type;
}

bool SocketImpl::isValid() const {
    return m_handle != INVALID_SOCKET_HANDLE;
}

SocketHandle SocketImpl::getHandle() const {
    return m_handle;
}

SocketError SocketImpl::getLastError() const {
    return m_lastError;
}

} // namespace dht_hunter::network
