#include "utils/network_utils.hpp"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

namespace dht_hunter::utils::network {

//=============================================================================
// UDPSocket Implementation
//=============================================================================

class UDPSocket::Impl {
public:
    Impl() : m_socket(-1), m_running(false) {}

    ~Impl() {
        close();
    }

    bool bind(uint16_t port) {
        // Create socket if it doesn't exist
        if (m_socket < 0) {
            m_socket = socket(AF_INET, SOCK_DGRAM, 0);
            if (m_socket < 0) {
                return false;
            }

            // Set non-blocking mode
            int flags = fcntl(m_socket, F_GETFL, 0);
            if (flags < 0) {
                close();
                return false;
            }
            if (fcntl(m_socket, F_SETFL, flags | O_NONBLOCK) < 0) {
                close();
                return false;
            }
        }

        // Bind to the specified port
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (::bind(m_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close();
            return false;
        }

        return true;
    }

    ssize_t sendTo(const void* data, size_t size, const std::string& address, uint16_t port) {
        if (m_socket < 0) {
            return -1;
        }

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
            return -1;
        }

        return sendto(m_socket, data, size, 0, (struct sockaddr*)&addr, sizeof(addr));
    }

    ssize_t receiveFrom(void* buffer, size_t size, std::string& address, uint16_t& port) {
        if (m_socket < 0) {
            return -1;
        }

        struct sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        std::memset(&addr, 0, addrLen);

        ssize_t received = recvfrom(m_socket, buffer, size, 0, (struct sockaddr*)&addr, &addrLen);
        if (received > 0) {
            char addrStr[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &addr.sin_addr, addrStr, INET_ADDRSTRLEN)) {
                address = addrStr;
                port = ntohs(addr.sin_port);
            } else {
                address = "";
                port = 0;
            }
        }

        return received;
    }

    bool startReceiveLoop() {
        if (m_socket < 0) {
            return false;
        }

        if (m_running) {
            return true;
        }

        m_running = true;
        m_receiveThread = std::thread([this]() {
            std::vector<uint8_t> buffer(DEFAULT_MTU_SIZE);
            std::string address;
            uint16_t port;

            while (m_running) {
                ssize_t received = receiveFrom(buffer.data(), buffer.size(), address, port);
                if (received > 0) {
                    if (m_receiveCallback) {
                        // Resize the buffer to the actual received size
                        buffer.resize(received);
                        m_receiveCallback(buffer, address, port);
                        // Restore the buffer size for the next receive
                        buffer.resize(DEFAULT_MTU_SIZE);
                    }
                } else if (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    // Error occurred
                    break;
                }

                // Sleep a bit to avoid busy-waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });

        return true;
    }

    void stopReceiveLoop() {
        m_running = false;
        if (m_receiveThread.joinable()) {
            m_receiveThread.join();
        }
    }

    void setReceiveCallback(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback) {
        m_receiveCallback = callback;
    }

    bool isValid() const {
        return m_socket >= 0;
    }

    void close() {
        stopReceiveLoop();
        if (m_socket >= 0) {
            ::close(m_socket);
            m_socket = -1;
        }
    }

private:
    int m_socket;
    std::atomic<bool> m_running;
    std::thread m_receiveThread;
    std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> m_receiveCallback;
};

UDPSocket::UDPSocket() : m_impl(std::make_unique<Impl>()) {
}

UDPSocket::~UDPSocket() {
}

bool UDPSocket::bind(uint16_t port) {
    return m_impl->bind(port);
}

ssize_t UDPSocket::sendTo(const void* data, size_t size, const std::string& address, uint16_t port) {
    return m_impl->sendTo(data, size, address, port);
}

ssize_t UDPSocket::receiveFrom(void* buffer, size_t size, std::string& address, uint16_t& port) {
    return m_impl->receiveFrom(buffer, size, address, port);
}

bool UDPSocket::startReceiveLoop() {
    return m_impl->startReceiveLoop();
}

void UDPSocket::stopReceiveLoop() {
    m_impl->stopReceiveLoop();
}

void UDPSocket::setReceiveCallback(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback) {
    m_impl->setReceiveCallback(callback);
}

bool UDPSocket::isValid() const {
    return m_impl->isValid();
}

void UDPSocket::close() {
    m_impl->close();
}

//=============================================================================
// UDPServer Implementation
//=============================================================================

UDPServer::UDPServer() : m_running(false) {
}

UDPServer::~UDPServer() {
    stop();
}

bool UDPServer::start(uint16_t port) {
    if (m_running) {
        return true;
    }

    if (!m_socket.bind(port)) {
        return false;
    }

    m_running = true;
    return m_socket.startReceiveLoop();
}

bool UDPServer::start() {
    return start(DEFAULT_UDP_PORT);
}

void UDPServer::stop() {
    if (!m_running) {
        return;
    }

    m_socket.stopReceiveLoop();
    m_socket.close();
    m_running = false;
}

bool UDPServer::isRunning() const {
    return m_running;
}

ssize_t UDPServer::sendTo(const std::vector<uint8_t>& data, const std::string& address, uint16_t port) {
    return m_socket.sendTo(data.data(), data.size(), address, port);
}

void UDPServer::setMessageHandler(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback) {
    m_socket.setReceiveCallback(callback);
}

//=============================================================================
// UDPClient Implementation
//=============================================================================

UDPClient::UDPClient() : m_running(false) {
}

UDPClient::~UDPClient() {
    stop();
}

bool UDPClient::start() {
    if (m_running) {
        return true;
    }

    // Bind to any available port
    if (!m_socket.bind(0)) {
        return false;
    }

    m_running = true;
    return m_socket.startReceiveLoop();
}

void UDPClient::stop() {
    if (!m_running) {
        return;
    }

    m_socket.stopReceiveLoop();
    m_socket.close();
    m_running = false;
}

bool UDPClient::isRunning() const {
    return m_running;
}

ssize_t UDPClient::send(const std::vector<uint8_t>& data, const std::string& address, uint16_t port) {
    return m_socket.sendTo(data.data(), data.size(), address, port);
}

void UDPClient::setMessageHandler(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback) {
    m_socket.setReceiveCallback(callback);
}

} // namespace dht_hunter::utils::network
