#include "dht_hunter/network/platform/socket_impl.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#ifdef _WIN32

DEFINE_COMPONENT_LOGGER("Network", "SocketImpl")

namespace dht_hunter::network {

// Windows-specific socket initialization
namespace {
    class WinsockInitializer {
    public:
        WinsockInitializer() {
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                throw std::runtime_error("Failed to initialize Winsock");
            }
            getLogger()->debug("Winsock initialized");
        }
        ~WinsockInitializer() {
            WSACleanup();
            getLogger()->debug("Winsock cleaned up");
        }
    };
    // Static instance to ensure initialization
    static WinsockInitializer winsockInitializer;
}
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
        getLogger()->debug("Socket closed: {}", static_cast<int>(m_handle));
        m_handle = INVALID_SOCKET_HANDLE;
    }
}
// Windows-specific non-blocking mode setting
bool SocketImpl::setNonBlocking(bool nonBlocking) {
    getLogger()->trace("Setting socket to {} mode", nonBlocking ? "non-blocking" : "blocking");
    
    if (!isValid()) {
        getLogger()->error("Cannot set non-blocking mode on invalid socket");
        m_lastError = SocketError::InvalidSocket;
        return false;
    }
    
    // Windows uses ioctlsocket with FIONBIO
    u_long mode = nonBlocking ? 1 : 0;
    if (ioctlsocket(m_handle, FIONBIO, &mode) != 0) {
        int errorCode = WSAGetLastError();
        getLogger()->error("Failed to set non-blocking mode: {}", getErrorString(translateError(errorCode)));
        m_lastError = translateError(errorCode);
        return false;
    }
    
    getLogger()->debug("Successfully set socket to {} mode", 
                     nonBlocking ? "non-blocking" : "blocking");
    return true;
}
// Windows-specific TCP keep alive implementation
bool TCPSocketImpl::setKeepAlive(bool keepAlive, int /* idleTime */, int /* interval */, int /* count */) {
    auto tcpLogger = dht_hunter::logforge::LogForge::getLogger("TCPSocketImpl");
    if (!isValid()) {
        tcpLogger->error("Cannot set keep alive on invalid socket");
        return false;
    }
    // Enable/disable keep alive
    int value = keepAlive ? 1 : 0;
    if (setsockopt(getHandle(), SOL_SOCKET, SO_KEEPALIVE,
                  reinterpret_cast<char*>(&value), sizeof(value)) != 0) {
        tcpLogger->error("Failed to set SO_KEEPALIVE: {}",
                     Socket::getErrorString(translateError(getLastErrorCode())));
        return false;
    }
    if (keepAlive) {
        // Windows-specific keep alive settings are commented out to avoid unused parameter warnings
        // Uncomment and implement as needed for specific platforms
        /*
        struct tcp_keepalive {
            ULONG onoff;
            ULONG keepalivetime;
            ULONG keepaliveinterval;
        };
        int keepIdleTime = idleTime;
        int keepInterval = interval;
        tcp_keepalive keepAliveSettings;
        keepAliveSettings.onoff = 1;
        keepAliveSettings.keepalivetime = keepIdleTime * 1000;
        keepAliveSettings.keepaliveinterval = keepInterval * 1000;
        DWORD bytesReturned = 0;
        // SIO_KEEPALIVE_VALS is 0x98000004
        if (WSAIoctl(getHandle(), 0x98000004, &keepAliveSettings,
                    sizeof(keepAliveSettings), nullptr, 0, &bytesReturned,
                    nullptr, nullptr) == SOCKET_ERROR) {
            getLogger()->error("Failed to set keep alive settings: {}",
                         Socket::getErrorString(translateError(getLastErrorCode())));
            return false;
        }
        */
    }
    getLogger()->debug("Socket keep alive {}", keepAlive ? "enabled" : "disabled");
    return true;
}
} // namespace dht_hunter::network
#endif // _WIN32
