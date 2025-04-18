#include "dht_hunter/network/platform/socket_impl.hpp"
#include "dht_hunter/logforge/logforge.hpp"

namespace dht_hunter::network {

namespace {
    // Get logger for socket implementation
    auto logger = dht_hunter::logforge::LogForge::getLogger("SocketImpl");
}

// Common Socket error string implementation
std::string Socket::getErrorString(SocketError error) {
    switch (error) {
        case SocketError::None:
            return "No error";
        case SocketError::InvalidSocket:
            return "Invalid socket";
        case SocketError::AddressInUse:
            return "Address already in use";
        case SocketError::ConnectionRefused:
            return "Connection refused";
        case SocketError::NetworkUnreachable:
            return "Network is unreachable";
        case SocketError::TimedOut:
            return "Connection timed out";
        case SocketError::WouldBlock:
            return "Operation would block";
        case SocketError::HostUnreachable:
            return "Host is unreachable";
        case SocketError::AddressNotAvailable:
            return "Address not available";
        case SocketError::NotConnected:
            return "Socket not connected";
        case SocketError::Interrupted:
            return "Operation interrupted";
        case SocketError::OperationInProgress:
            return "Operation already in progress";
        case SocketError::AlreadyConnected:
            return "Socket already connected";
        case SocketError::NotBound:
            return "Socket not bound";
        case SocketError::Unknown:
        default:
            return "Unknown error";
    }
}

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
