#include "dht_hunter/network/tcp_client.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"

#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

namespace dht_hunter::network {

TCPClient::TCPClient()
    : m_socket(-1),
      m_connected(false),
      m_running(false) {

    // Initialize the receive buffer
    m_receiveBuffer.resize(4096);

    // Initialize connection statistics
    m_stats.reset();
}

TCPClient::~TCPClient() {
    // Stop the receive thread
    m_running = false;

    // Close the socket
    if (m_socket >= 0) {
        close(m_socket);
        m_socket = -1;
    }

    m_connected = false;

    // Join the receive thread
    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }
}

bool TCPClient::connect(const std::string& ip, uint16_t port, int timeoutSec) {
    // Check if already connected
    if (m_connected) {
        return true;
    }

    // Reset connection statistics
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.reset();
        m_stats.connectTime = std::chrono::steady_clock::now();
    }

    // Create a socket
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < 0) {
        handleError("Failed to create socket: " + std::string(strerror(errno)));
        return false;
    }

    // Set non-blocking mode
    int flags = fcntl(m_socket, F_GETFL, 0);
    if (flags < 0) {
        handleError("Failed to get socket flags: " + std::string(strerror(errno)));
        close(m_socket);
        m_socket = -1;
        return false;
    }

    if (fcntl(m_socket, F_SETFL, flags | O_NONBLOCK) < 0) {
        handleError("Failed to set socket to non-blocking mode: " + std::string(strerror(errno)));
        close(m_socket);
        m_socket = -1;
        return false;
    }

    // Set up the server address
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0) {
        handleError("Invalid address: " + ip);
        close(m_socket);
        m_socket = -1;
        return false;
    }

    // Connect to the server (non-blocking)
    int result = ::connect(m_socket, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr));
    if (result < 0) {
        if (errno != EINPROGRESS) {
            handleError("Failed to connect: " + std::string(strerror(errno)));
            close(m_socket);
            m_socket = -1;
            return false;
        }

        // Wait for the connection to complete
        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(m_socket, &writeSet);

        struct timeval timeout;
        timeout.tv_sec = timeoutSec;
        timeout.tv_usec = 0;

        unified_event::logDebug("Network.TCPClient", "Attempting to connect to " + ip + ":" + std::to_string(port) +
                              " with " + std::to_string(timeoutSec) + " second timeout");

        result = select(m_socket + 1, nullptr, &writeSet, nullptr, &timeout);
        if (result <= 0) {
            if (result == 0) {
                handleError("Connection timed out");
            } else {
                handleError("Select failed: " + std::string(strerror(errno)));
            }
            close(m_socket);
            m_socket = -1;
            return false;
        }

        // Check if the connection was successful
        int error = 0;
        socklen_t errorLen = sizeof(error);
        if (getsockopt(m_socket, SOL_SOCKET, SO_ERROR, &error, &errorLen) < 0 || error != 0) {
            if (error != 0) {
                handleError("Connection failed: " + std::string(strerror(error)));
            } else {
                handleError("Failed to get socket error: " + std::string(strerror(errno)));
            }
            close(m_socket);
            m_socket = -1;
            return false;
        }
    }

    // Set the socket back to blocking mode
    if (fcntl(m_socket, F_SETFL, flags) < 0) {
        handleError("Failed to set socket back to blocking mode: " + std::string(strerror(errno)));
        close(m_socket);
        m_socket = -1;
        return false;
    }

    // Start the receive thread
    m_connected = true;
    m_running = true;
    m_receiveThread = std::thread(&TCPClient::receiveData, this);

    // Update connection statistics
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        auto now = std::chrono::steady_clock::now();
        m_stats.connectDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_stats.connectTime);
        m_stats.lastActivityTime = now;
    }

    unified_event::logDebug("Network.TCPClient", "Successfully connected to " + ip + ":" + std::to_string(port) +
                          " in " + std::to_string(m_stats.connectDuration.count()) + "ms");

    return true;
}

void TCPClient::disconnect() {
    // Stop the receive thread
    m_running = false;

    // Close the socket
    if (m_socket >= 0) {
        close(m_socket);
        m_socket = -1;
    }

    m_connected = false;

    // Join the receive thread
    if (m_receiveThread.joinable()) {
        try {
            m_receiveThread.join();
        } catch (const std::system_error& e) {
            // Ignore the error if the thread is not joinable
            if (e.code() != std::errc::invalid_argument &&
                e.code() != std::errc::resource_deadlock_would_occur) {
                throw;
            }
        }
    }
}

bool TCPClient::isConnected() const {
    return m_connected;
}

bool TCPClient::send(const uint8_t* data, size_t length) {
    if (!m_connected || m_socket < 0) {
        return false;
    }

    size_t totalSent = 0;
    while (totalSent < length) {
        ssize_t sent = ::send(m_socket, data + totalSent, length - totalSent, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                // Interrupted, try again
                continue;
            }

            handleError("Failed to send data: " + std::string(strerror(errno)));
            return false;
        }

        totalSent += static_cast<size_t>(sent);
    }

    // Update statistics
    updateStatsForDataSent(totalSent);

    return true;
}

void TCPClient::setDataReceivedCallback(std::function<void(const uint8_t*, size_t)> callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_dataReceivedCallback = callback;
}

void TCPClient::setErrorCallback(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_errorCallback = callback;
}

void TCPClient::setConnectionClosedCallback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_connectionClosedCallback = callback;
}

void TCPClient::receiveData() {
    while (m_running && m_connected) {
        // Receive data
        ssize_t received = recv(m_socket, m_receiveBuffer.data(), m_receiveBuffer.size(), 0);
        if (received < 0) {
            if (errno == EINTR) {
                // Interrupted, try again
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available, wait a bit
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            handleError("Failed to receive data: " + std::string(strerror(errno)));
            break;
        } else if (received == 0) {
            // Connection closed
            handleConnectionClosed();
            break;
        }

        // Update statistics
        updateStatsForDataReceived(static_cast<size_t>(received));

        // Call the data received callback
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        if (m_dataReceivedCallback) {
            m_dataReceivedCallback(m_receiveBuffer.data(), static_cast<size_t>(received));
        }
    }

    m_connected = false;
}

void TCPClient::handleError(const std::string& error) {
    // Update error statistics
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.errorCount++;
        m_stats.lastActivityTime = std::chrono::steady_clock::now();
    }

    std::lock_guard<std::mutex> lock(m_callbackMutex);
    if (m_errorCallback) {
        m_errorCallback(error);
    }
}

void TCPClient::handleConnectionClosed() {
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.lastActivityTime = std::chrono::steady_clock::now();
    }

    std::lock_guard<std::mutex> lock(m_callbackMutex);
    if (m_connectionClosedCallback) {
        m_connectionClosedCallback();
    }
}

const ConnectionStats& TCPClient::getStats() const {
    return m_stats;
}

void TCPClient::updateStatsForDataReceived(size_t bytesCount) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats.bytesReceived += bytesCount;
    m_stats.lastActivityTime = std::chrono::steady_clock::now();
}

void TCPClient::updateStatsForDataSent(size_t bytesCount) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats.bytesSent += bytesCount;
    m_stats.lastActivityTime = std::chrono::steady_clock::now();
}

void TCPClient::updateLatency(float latencyMs) {
    std::lock_guard<std::mutex> lock(m_statsMutex);

    // Add to history
    m_latencyHistory.push_back(latencyMs);

    // Keep history size limited
    if (m_latencyHistory.size() > MAX_LATENCY_HISTORY) {
        m_latencyHistory.pop_front();
    }

    // Calculate average latency
    if (!m_latencyHistory.empty()) {
        float sum = 0.0f;
        for (float latency : m_latencyHistory) {
            sum += latency;
        }
        m_stats.latencyMs = sum / m_latencyHistory.size();
    }
}

} // namespace dht_hunter::network
