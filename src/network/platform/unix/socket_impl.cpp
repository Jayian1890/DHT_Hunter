#include "dht_hunter/network/platform/socket_impl.hpp"
#include "dht_hunter/logforge/logforge.hpp"

#ifndef _WIN32

namespace dht_hunter::network {

namespace {
    // Get logger for socket implementation
    auto logger = dht_hunter::logforge::LogForge::getLogger("SocketImpl");
    
    // Ignore SIGPIPE on Unix-like systems
    class SignalHandler {
    public:
        SignalHandler() {
            signal(SIGPIPE, SIG_IGN);
            logger->debug("SIGPIPE ignored");
        }
    };
    
    // Static instance to ensure initialization
    static SignalHandler signalHandler;
}

// Unix-specific error code translation
SocketError SocketImpl::translateError(int errorCode) {
    switch (errorCode) {
        case ENOTSOCK:
            return SocketError::InvalidSocket;
        case EADDRINUSE:
            return SocketError::AddressInUse;
        case ECONNREFUSED:
            return SocketError::ConnectionRefused;
        case ENETUNREACH:
            return SocketError::NetworkUnreachable;
        case ETIMEDOUT:
            return SocketError::TimedOut;
        case EWOULDBLOCK:
        case EAGAIN:
            return SocketError::WouldBlock;
        case EHOSTUNREACH:
            return SocketError::HostUnreachable;
        case EADDRNOTAVAIL:
            return SocketError::AddressNotAvailable;
        case ENOTCONN:
            return SocketError::NotConnected;
        case EINTR:
            return SocketError::Interrupted;
        case EALREADY:
            return SocketError::OperationInProgress;
        case EISCONN:
            return SocketError::AlreadyConnected;
        default:
            return SocketError::Unknown;
    }
}

// Unix-specific error code retrieval
int SocketImpl::getLastErrorCode() {
    return errno;
}

// Unix-specific socket close
void SocketImpl::close() {
    if (m_handle != INVALID_SOCKET_HANDLE) {
        ::close(m_handle);
        logger->debug("Socket closed: {}", static_cast<int>(m_handle));
        m_handle = INVALID_SOCKET_HANDLE;
    }
}

// Unix-specific non-blocking mode setting
bool SocketImpl::setNonBlocking(bool nonBlocking) {
    if (m_handle == INVALID_SOCKET_HANDLE) {
        m_lastError = SocketError::InvalidSocket;
        logger->error("Cannot set non-blocking mode on invalid socket");
        return false;
    }
    
    int flags = fcntl(m_handle, F_GETFL, 0);
    if (flags == -1) {
        m_lastError = translateError(getLastErrorCode());
        logger->error("Failed to get socket flags: {}", getErrorString(m_lastError));
        return false;
    }
    
    if (nonBlocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    
    if (fcntl(m_handle, F_SETFL, flags) == -1) {
        m_lastError = translateError(getLastErrorCode());
        logger->error("Failed to set non-blocking mode: {}", getErrorString(m_lastError));
        return false;
    }
    
    logger->debug("Socket set to {} mode", nonBlocking ? "non-blocking" : "blocking");
    return true;
}

// Unix-specific TCP keep alive implementation
bool TCPSocketImpl::setKeepAlive(bool keepAlive, int idleTime, int interval, int count) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("TCPSocketImpl");
    
    if (!isValid()) {
        logger->error("Cannot set keep alive on invalid socket");
        return false;
    }
    
    // Enable/disable keep alive
    int value = keepAlive ? 1 : 0;
    if (setsockopt(getHandle(), SOL_SOCKET, SO_KEEPALIVE, 
                  reinterpret_cast<char*>(&value), sizeof(value)) != 0) {
        logger->error("Failed to set SO_KEEPALIVE: {}", 
                     Socket::getErrorString(translateError(getLastErrorCode())));
        return false;
    }
    
    if (keepAlive) {
        // Unix-specific keep alive settings
        #ifdef TCP_KEEPIDLE
        if (setsockopt(getHandle(), IPPROTO_TCP, TCP_KEEPIDLE, 
                      &idleTime, sizeof(idleTime)) != 0) {
            logger->error("Failed to set TCP_KEEPIDLE: {}", 
                         Socket::getErrorString(translateError(getLastErrorCode())));
            return false;
        }
        #endif
        
        #ifdef TCP_KEEPINTVL
        if (setsockopt(getHandle(), IPPROTO_TCP, TCP_KEEPINTVL, 
                      &interval, sizeof(interval)) != 0) {
            logger->error("Failed to set TCP_KEEPINTVL: {}", 
                         Socket::getErrorString(translateError(getLastErrorCode())));
            return false;
        }
        #endif
        
        #ifdef TCP_KEEPCNT
        if (setsockopt(getHandle(), IPPROTO_TCP, TCP_KEEPCNT, 
                      &count, sizeof(count)) != 0) {
            logger->error("Failed to set TCP_KEEPCNT: {}", 
                         Socket::getErrorString(translateError(getLastErrorCode())));
            return false;
        }
        #endif
    }
    
    logger->debug("Socket keep alive {}", keepAlive ? "enabled" : "disabled");
    return true;
}

} // namespace dht_hunter::network

#endif // !_WIN32
