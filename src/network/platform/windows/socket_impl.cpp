#include "dht_hunter/network/platform/socket_impl.hpp"
#include "dht_hunter/logforge/logforge.hpp"

#ifdef _WIN32

namespace dht_hunter::network {

namespace {
    // Get logger for socket implementation
    auto logger = dht_hunter::logforge::LogForge::getLogger("SocketImpl");
    
    // Initialize Winsock on Windows
    class WinsockInitializer {
    public:
        WinsockInitializer() {
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                logger->critical("Failed to initialize Winsock");
                throw std::runtime_error("Failed to initialize Winsock");
            }
            logger->debug("Winsock initialized");
        }
        
        ~WinsockInitializer() {
            WSACleanup();
            logger->debug("Winsock cleaned up");
        }
    };
    
    // Static instance to ensure initialization
    static WinsockInitializer winsockInitializer;
}

// Windows-specific error code translation
SocketError SocketImpl::translateError(int errorCode) {
    switch (errorCode) {
        case WSAENOTSOCK:
            return SocketError::InvalidSocket;
        case WSAEADDRINUSE:
            return SocketError::AddressInUse;
        case WSAECONNREFUSED:
            return SocketError::ConnectionRefused;
        case WSAENETUNREACH:
            return SocketError::NetworkUnreachable;
        case WSAETIMEDOUT:
            return SocketError::TimedOut;
        case WSAEWOULDBLOCK:
            return SocketError::WouldBlock;
        case WSAEHOSTUNREACH:
            return SocketError::HostUnreachable;
        case WSAEADDRNOTAVAIL:
            return SocketError::AddressNotAvailable;
        case WSAENOTCONN:
            return SocketError::NotConnected;
        case WSAEINTR:
            return SocketError::Interrupted;
        case WSAEALREADY:
            return SocketError::OperationInProgress;
        case WSAEISCONN:
            return SocketError::AlreadyConnected;
        default:
            return SocketError::Unknown;
    }
}

// Windows-specific error code retrieval
int SocketImpl::getLastErrorCode() {
    return WSAGetLastError();
}

// Windows-specific socket close
void SocketImpl::close() {
    if (m_handle != INVALID_SOCKET_HANDLE) {
        closesocket(m_handle);
        logger->debug("Socket closed: {}", static_cast<int>(m_handle));
        m_handle = INVALID_SOCKET_HANDLE;
    }
}

// Windows-specific non-blocking mode setting
bool SocketImpl::setNonBlocking(bool nonBlocking) {
    if (m_handle == INVALID_SOCKET_HANDLE) {
        m_lastError = SocketError::InvalidSocket;
        logger->error("Cannot set non-blocking mode on invalid socket");
        return false;
    }
    
    u_long mode = nonBlocking ? 1 : 0;
    if (ioctlsocket(m_handle, FIONBIO, &mode) != 0) {
        m_lastError = translateError(getLastErrorCode());
        logger->error("Failed to set non-blocking mode: {}", getErrorString(m_lastError));
        return false;
    }
    
    logger->debug("Socket set to {} mode", nonBlocking ? "non-blocking" : "blocking");
    return true;
}

// Windows-specific TCP keep alive implementation
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
        // Windows-specific keep alive settings
        tcp_keepalive keepAliveSettings;
        keepAliveSettings.onoff = 1;
        keepAliveSettings.keepalivetime = idleTime * 1000;
        keepAliveSettings.keepaliveinterval = interval * 1000;
        
        DWORD bytesReturned = 0;
        if (WSAIoctl(getHandle(), SIO_KEEPALIVE_VALS, &keepAliveSettings, 
                    sizeof(keepAliveSettings), nullptr, 0, &bytesReturned, 
                    nullptr, nullptr) == SOCKET_ERROR) {
            logger->error("Failed to set keep alive settings: {}", 
                         Socket::getErrorString(translateError(getLastErrorCode())));
            return false;
        }
    }
    
    logger->debug("Socket keep alive {}", keepAlive ? "enabled" : "disabled");
    return true;
}

} // namespace dht_hunter::network

#endif // _WIN32
