#include "dht_hunter/network/udp_socket.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"

#include <thread>
#include <atomic>
#include <mutex>
#include <array>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    // Define a function instead of a macro for closesocket
    inline int closesocket(int s) { return ::close(s); }
#endif

namespace dht_hunter::network {

// Platform-specific initialization
#ifdef _WIN32
class WSAInitializer {
public:
    WSAInitializer() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("Failed to initialize Winsock");
        }
    }

    ~WSAInitializer() {
        WSACleanup();
    }
};

static WSAInitializer g_wsaInitializer;
#endif

class UDPSocket::Impl {
public:
    Impl() : m_socket(INVALID_SOCKET), m_running(false) {
        unified_event::logTrace("Network.UDPSocket", "TRACE: Impl constructor entry");
        // Create the socket
        m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_socket == INVALID_SOCKET) {
            unified_event::logTrace("Network.UDPSocket", "TRACE: Failed to create socket");
        } else {
            unified_event::logTrace("Network.UDPSocket", "TRACE: Socket created successfully");
        }
        unified_event::logTrace("Network.UDPSocket", "TRACE: Impl constructor exit");
    }

    ~Impl() {
        close();
    }

    bool bind(uint16_t port) {
        unified_event::logTrace("Network.UDPSocket", "TRACE: bind() entry with port " + std::to_string(port));

        if (m_socket == INVALID_SOCKET) {
            unified_event::logTrace("Network.UDPSocket", "TRACE: Cannot bind - socket is invalid");
            return false;
        }

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        unified_event::logTrace("Network.UDPSocket", "TRACE: Calling ::bind");
        if (::bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            unified_event::logTrace("Network.UDPSocket", "TRACE: ::bind failed with error");
            return false;
        }

        unified_event::logTrace("Network.UDPSocket", "TRACE: bind() successful");
        return true;
    }

    ssize_t sendTo(const void* data, size_t size, const std::string& address, uint16_t port) {    // Logger initialization removed

        if (m_socket == INVALID_SOCKET) {
            return -1;
        }

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
            return -1;
        }

        auto result = sendto(m_socket, static_cast<const char*>(data), static_cast<socklen_t>(size), 0,
                           reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

        if (result == SOCKET_ERROR) {
            return -1;
        }
        return result;
    }

    ssize_t receiveFrom(void* buffer, size_t size, std::string& address, uint16_t& port) {    // Logger initialization removed

        if (m_socket == INVALID_SOCKET) {
            return -1;
        }

        sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);

        auto result = recvfrom(m_socket, static_cast<char*>(buffer), static_cast<socklen_t>(size), 0,
                             reinterpret_cast<sockaddr*>(&addr), &addrLen);

        if (result == SOCKET_ERROR) {
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                return 0;  // No data available
            }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;  // No data available
            }
#endif
            return -1;
        }

        char addrStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, addrStr, INET_ADDRSTRLEN);
        address = addrStr;
        port = ntohs(addr.sin_port);
        return result;
    }

    void setReceiveCallback(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback) {
        try {
            utility::thread::withLock(m_callbackMutex, [this, callback = std::move(callback)]() mutable {
                m_receiveCallback = std::move(callback);
            }, "UDPSocket::m_callbackMutex");
        } catch (const utility::thread::LockTimeoutException& e) {
            // Log the error
            unified_event::logError("Network.UDPSocket", e.what());
        }
    }

    bool startReceiveLoop() {
        unified_event::logTrace("Network.UDPSocket", "TRACE: startReceiveLoop() entry");

        if (m_socket == INVALID_SOCKET) {
            unified_event::logTrace("Network.UDPSocket", "TRACE: Cannot start receive loop - socket is invalid");
            return false;
        }

        if (m_running) {
            unified_event::logTrace("Network.UDPSocket", "TRACE: Receive loop already running");
            return true;
        }

        // Set socket to non-blocking mode
        unified_event::logTrace("Network.UDPSocket", "TRACE: Setting socket to non-blocking mode");
#ifdef _WIN32
        u_long mode = 1;
        if (ioctlsocket(m_socket, FIONBIO, &mode) != 0) {
            unified_event::logTrace("Network.UDPSocket", "TRACE: Failed to set socket to non-blocking mode");
            return false;
        }
#else
        int flags = fcntl(m_socket, F_GETFL, 0);
        if (flags == -1) {
            unified_event::logTrace("Network.UDPSocket", "TRACE: Failed to get socket flags");
            return false;
        }
        if (fcntl(m_socket, F_SETFL, flags | O_NONBLOCK) == -1) {
            unified_event::logTrace("Network.UDPSocket", "TRACE: Failed to set socket to non-blocking mode");
            return false;
        }
#endif
        unified_event::logTrace("Network.UDPSocket", "TRACE: Socket set to non-blocking mode");

        unified_event::logTrace("Network.UDPSocket", "TRACE: Setting m_running to true");
        m_running = true;

        unified_event::logTrace("Network.UDPSocket", "TRACE: Starting receive thread");
        m_receiveThread = std::thread(&UDPSocket::Impl::receiveLoop, this);
        unified_event::logTrace("Network.UDPSocket", "TRACE: Receive thread started");

        unified_event::logTrace("Network.UDPSocket", "TRACE: startReceiveLoop() successful");
        return true;
    }

    void stopReceiveLoop() {
        unified_event::logTrace("Network.UDPSocket", "TRACE: stopReceiveLoop() entry");

        if (!m_running) {
            unified_event::logTrace("Network.UDPSocket", "TRACE: Receive loop not running");
            return;
        }

        unified_event::logTrace("Network.UDPSocket", "TRACE: Setting m_running to false");
        m_running = false;

        if (m_receiveThread.joinable()) {
            unified_event::logTrace("Network.UDPSocket", "TRACE: Joining receive thread");
            m_receiveThread.join();
            unified_event::logTrace("Network.UDPSocket", "TRACE: Receive thread joined");
        } else {
            unified_event::logTrace("Network.UDPSocket", "TRACE: Receive thread not joinable");
        }

        unified_event::logTrace("Network.UDPSocket", "TRACE: stopReceiveLoop() exit");
    }

    bool isValid() const {
        return m_socket != INVALID_SOCKET;
    }

    void close() {    // Logger initialization removed

        stopReceiveLoop();

        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
    }

private:
    void receiveLoop() {
        unified_event::logTrace("Network.UDPSocket", "TRACE: receiveLoop() entry");

        std::array<uint8_t, 65536> buffer;
        unified_event::logTrace("Network.UDPSocket", "TRACE: Created receive buffer of size " + std::to_string(buffer.size()));

        while (m_running) {
            std::string address;
            uint16_t port = 0;

            // Don't log waiting for data to avoid log spam
            ssize_t bytesReceived = receiveFrom(buffer.data(), buffer.size(), address, port);

            if (bytesReceived > 0) {
                unified_event::logTrace("Network.UDPSocket", "TRACE: Received " + std::to_string(bytesReceived) + " bytes from " + address + ":" + std::to_string(port));
                std::vector<uint8_t> data(buffer.data(), buffer.data() + bytesReceived);

                try {
                    unified_event::logTrace("Network.UDPSocket", "TRACE: Acquiring callback mutex");
                    utility::thread::withLock(m_callbackMutex, [this, &data, &address, port]() {
                        unified_event::logTrace("Network.UDPSocket", "TRACE: Callback mutex acquired");
                        if (m_receiveCallback) {
                            unified_event::logTrace("Network.UDPSocket", "TRACE: Calling receive callback");
                            try {
                                m_receiveCallback(data, address, port);
                                unified_event::logTrace("Network.UDPSocket", "TRACE: Receive callback completed");
                            } catch (const std::exception& e) {
                                unified_event::logError("Network.UDPSocket", "Exception in receive callback: " + std::string(e.what()));
                                unified_event::logTrace("Network.UDPSocket", "TRACE: Exception in receive callback: " + std::string(e.what()));
                            }
                        } else {
                            unified_event::logTrace("Network.UDPSocket", "TRACE: No receive callback registered");
                        }
                    }, "UDPSocket::m_callbackMutex");
                } catch (const utility::thread::LockTimeoutException& e) {
                    unified_event::logError("Network.UDPSocket", "Failed to acquire lock in receive loop: " + std::string(e.what()));
                    unified_event::logTrace("Network.UDPSocket", "TRACE: Lock timeout exception: " + std::string(e.what()));
                }
            } else if (bytesReceived < 0) {
                unified_event::logTrace("Network.UDPSocket", "TRACE: Error receiving data, exiting receive loop");
                // Error occurred
                break;
            }

            // Sleep a bit to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        unified_event::logTrace("Network.UDPSocket", "TRACE: receiveLoop() exit");
    }

    SOCKET m_socket;
    std::atomic<bool> m_running;
    std::thread m_receiveThread;
    std::recursive_mutex m_callbackMutex;
    std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> m_receiveCallback;
};

// UDPSocket implementation
UDPSocket::UDPSocket() : m_impl(std::make_unique<Impl>()) {
}

UDPSocket::~UDPSocket() = default;

bool UDPSocket::bind(uint16_t port) {
    return m_impl->bind(port);
}

ssize_t UDPSocket::sendTo(const void* data, size_t size, const std::string& address, uint16_t port) {
    return m_impl->sendTo(data, size, address, port);
}

ssize_t UDPSocket::receiveFrom(void* buffer, size_t size, std::string& address, uint16_t& port) {
    return m_impl->receiveFrom(buffer, size, address, port);
}

void UDPSocket::setReceiveCallback(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback) {
    m_impl->setReceiveCallback(std::move(callback));
}

bool UDPSocket::startReceiveLoop() {
    return m_impl->startReceiveLoop();
}

void UDPSocket::stopReceiveLoop() {
    m_impl->stopReceiveLoop();
}

bool UDPSocket::isValid() const {
    return m_impl->isValid();
}

void UDPSocket::close() {
    m_impl->close();
}

} // namespace dht_hunter::network
