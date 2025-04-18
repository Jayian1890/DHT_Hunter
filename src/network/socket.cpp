#include "dht_hunter/network/socket.hpp"
#include "dht_hunter/logforge/logforge.hpp"

namespace dht_hunter::network {

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

} // namespace dht_hunter::network
