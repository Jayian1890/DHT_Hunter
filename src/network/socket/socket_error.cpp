#include "dht_hunter/network/socket/socket_error.hpp"
#include <cerrno>
#include <cstring>
#include <string>

namespace dht_hunter::network {

std::string socketErrorToString(SocketError error) {
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
        case SocketError::HostUnreachable:
            return "Host is unreachable";
        case SocketError::ConnectionReset:
            return "Connection reset by peer";
        case SocketError::ConnectionAborted:
            return "Connection aborted";
        case SocketError::ConnectionTimedOut:
            return "Connection timed out";
        case SocketError::NotConnected:
            return "Socket is not connected";
        case SocketError::AlreadyConnected:
            return "Socket is already connected";
        case SocketError::WouldBlock:
            return "Operation would block";
        case SocketError::InProgress:
            return "Operation now in progress";
        case SocketError::Interrupted:
            return "Interrupted function call";
        case SocketError::InvalidArgument:
            return "Invalid argument";
        case SocketError::NoMemory:
            return "Not enough memory";
        case SocketError::AccessDenied:
            return "Permission denied";
        case SocketError::NotSupported:
            return "Operation not supported";
        case SocketError::NotInitialized:
            return "Socket not initialized";
        case SocketError::AddressNotAvailable:
            return "Address not available";
        case SocketError::NetworkDown:
            return "Network is down";
        case SocketError::NetworkReset:
            return "Network dropped connection on reset";
        case SocketError::NoBufferSpace:
            return "No buffer space available";
        case SocketError::ProtocolNotSupported:
            return "Protocol not supported";
        case SocketError::OperationNotSupported:
            return "Operation not supported on socket";
        case SocketError::AddressFamilyNotSupported:
            return "Address family not supported";
        case SocketError::SystemLimitReached:
            return "System limit reached";
        case SocketError::Unknown:
        default:
            return "Unknown error";
    }
}

SocketError getLastSocketError() {
    int error = errno;
    switch (error) {
        case 0:
            return SocketError::None;
        case EBADF:
            return SocketError::InvalidSocket;
        case EADDRINUSE:
            return SocketError::AddressInUse;
        case ECONNREFUSED:
            return SocketError::ConnectionRefused;
        case ENETUNREACH:
            return SocketError::NetworkUnreachable;
        case EHOSTUNREACH:
            return SocketError::HostUnreachable;
        case ECONNRESET:
            return SocketError::ConnectionReset;
        case ECONNABORTED:
            return SocketError::ConnectionAborted;
        case ETIMEDOUT:
            return SocketError::ConnectionTimedOut;
        case ENOTCONN:
            return SocketError::NotConnected;
        case EISCONN:
            return SocketError::AlreadyConnected;
        case EWOULDBLOCK:
        case EAGAIN:
            return SocketError::WouldBlock;
        case EINPROGRESS:
            return SocketError::InProgress;
        case EINTR:
            return SocketError::Interrupted;
        case EINVAL:
            return SocketError::InvalidArgument;
        case ENOMEM:
            return SocketError::NoMemory;
        case EACCES:
            return SocketError::AccessDenied;
        case EOPNOTSUPP:
            return SocketError::NotSupported;
        case EADDRNOTAVAIL:
            return SocketError::AddressNotAvailable;
        case ENETDOWN:
            return SocketError::NetworkDown;
        case ENETRESET:
            return SocketError::NetworkReset;
        case ENOBUFS:
            return SocketError::NoBufferSpace;
        case EPROTONOSUPPORT:
            return SocketError::ProtocolNotSupported;
        case EAFNOSUPPORT:
            return SocketError::AddressFamilyNotSupported;
        case EMFILE:
            return SocketError::SystemLimitReached;
        default:
            return SocketError::Unknown;
    }
}

std::string getLastSocketErrorMessage() {
    return std::string(strerror(errno));
}

} // namespace dht_hunter::network
