#include "dht_hunter/network/udp_socket.hpp"
#include "dht_hunter/utils/lock_utils.hpp"
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
        // Create the socket
        m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_socket == INVALID_SOCKET) {    // Logger initialization removed
        }
    }

    ~Impl() {
        close();
    }

    bool bind(uint16_t port) {    // Logger initialization removed

        if (m_socket == INVALID_SOCKET) {
            return false;
        }

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (::bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            return false;
        }

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
            utils::withLock(m_callbackMutex, [this, callback = std::move(callback)]() mutable {
                m_receiveCallback = std::move(callback);
            }, "UDPSocket::m_callbackMutex");
        } catch (const utils::LockTimeoutException& e) {
            // Log the error
            unified_event::logError("Network.UDPSocket", e.what());
        }
    }

    bool startReceiveLoop() {    // Logger initialization removed

        if (m_socket == INVALID_SOCKET) {
            return false;
        }

        if (m_running) {
            return true;
        }

        // Set socket to non-blocking mode
#ifdef _WIN32
        u_long mode = 1;
        if (ioctlsocket(m_socket, FIONBIO, &mode) != 0) {
            return false;
        }
#else
        int flags = fcntl(m_socket, F_GETFL, 0);
        if (flags == -1) {
            return false;
        }
        if (fcntl(m_socket, F_SETFL, flags | O_NONBLOCK) == -1) {
            return false;
        }
#endif

        m_running = true;
        m_receiveThread = std::thread(&UDPSocket::Impl::receiveLoop, this);

        return true;
    }

    void stopReceiveLoop() {    // Logger initialization removed

        if (!m_running) {
            return;
        }

        m_running = false;

        if (m_receiveThread.joinable()) {
            m_receiveThread.join();
        }
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
    void receiveLoop() {    // Logger initialization removed

        std::array<uint8_t, 65536> buffer;

        while (m_running) {
            std::string address;
            uint16_t port = 0;

            ssize_t bytesReceived = receiveFrom(buffer.data(), buffer.size(), address, port);

            if (bytesReceived > 0) {
                std::vector<uint8_t> data(buffer.data(), buffer.data() + bytesReceived);

                try {
                    utils::withLock(m_callbackMutex, [this, &data, &address, port]() {
                        if (m_receiveCallback) {
                            try {
                                m_receiveCallback(data, address, port);
                            } catch (const std::exception& e) {
                                unified_event::logError("Network.UDPSocket", "Exception in receive callback: " + std::string(e.what()));
                            }
                        }
                    }, "UDPSocket::m_callbackMutex");
                } catch (const utils::LockTimeoutException& e) {
                    unified_event::logError("Network.UDPSocket", "Failed to acquire lock in receive loop: " + std::string(e.what()));
                }
            } else if (bytesReceived < 0) {
                // Error occurred
                break;
            }

            // Sleep a bit to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    SOCKET m_socket;
    std::atomic<bool> m_running;
    std::thread m_receiveThread;
    std::mutex m_callbackMutex;
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
