#include "dht_hunter/dht/network/dht_socket_manager.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include "dht_hunter/network/platform/socket_impl.hpp"
#include <vector>

DEFINE_COMPONENT_LOGGER("DHT", "SocketManager")

namespace dht_hunter::dht {

DHTSocketManager::DHTSocketManager(const DHTNodeConfig& config)
    : m_config(config), m_port(config.getPort()), m_running(false) {
    getLogger()->info("Creating socket manager for port {}", m_port);
}

DHTSocketManager::~DHTSocketManager() {
    stop();
}

bool DHTSocketManager::start(MessageReceivedCallback messageCallback) {
    if (m_running) {
        getLogger()->warning("Socket manager already running");
        return false;
    }

    if (!messageCallback) {
        getLogger()->error("No message callback provided");
        return false;
    }

    m_messageCallback = messageCallback;

    // Create UDP socket with proper locking
    {
        std::lock_guard<util::CheckedMutex> lock(m_socketMutex);
        m_socket = network::SocketFactory::createUDPSocket();
        
        // Bind to port
        if (!m_socket->bind(network::EndPoint(network::NetworkAddress::any(), m_port))) {
            getLogger()->error("Failed to bind socket to port {}: {}", m_port,
                         network::Socket::getErrorString(m_socket->getLastError()));
            return false;
        }
    }

    // Set non-blocking mode
    // Attempt to set non-blocking mode, but continue even if it fails
    // This is a workaround for the "Failed to set non-blocking mode: No error" issue on macOS
    if (!m_socket->setNonBlocking(true)) {
        // Check if the error is "No error" (which is a known issue on macOS)
        if (m_socket->getLastError() == network::SocketError::None) {
            getLogger()->warning("Ignoring 'No error' when setting non-blocking mode (known macOS issue)");
            // Continue anyway - the socket will likely work fine
        } else {
            getLogger()->error("Failed to set non-blocking mode: {}",
                         network::Socket::getErrorString(m_socket->getLastError()));
            // Only return false for real errors, not the "No error" issue
            return false;
        }
    }

    // Start the receive thread
    m_running = true;
    m_receiveThread = std::thread(&DHTSocketManager::receiveMessages, this);

    getLogger()->info("Socket manager started on port {}", m_port);
    return true;
}

void DHTSocketManager::stop() {
    // Use atomic operation to prevent multiple stops
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false)) {
        // Already stopped or stopping
        return;
    }

    getLogger()->info("Stopping socket manager");

    // Close the socket to unblock the receive thread with proper locking
    {
        std::lock_guard<util::CheckedMutex> lock(m_socketMutex);
        if (m_socket) {
            getLogger()->debug("Closing socket to unblock receive thread");
            m_socket->close();
            // Reset socket immediately to ensure no other threads can use it
            m_socket.reset();
        }
    }

    // Join the receive thread with timeout
    if (m_receiveThread.joinable()) {
        getLogger()->debug("Waiting for receive thread to join...");
        
        // Try to join with timeout
        auto joinStartTime = std::chrono::steady_clock::now();
        auto threadJoinTimeout = std::chrono::seconds(DEFAULT_THREAD_JOIN_TIMEOUT_SECONDS);
        
        std::thread tempThread([this]() {
            if (m_receiveThread.joinable()) {
                m_receiveThread.join();
            }
        });
        
        // Wait for the join thread with timeout
        if (tempThread.joinable()) {
            bool joined = false;
            while (!joined && 
                   std::chrono::steady_clock::now() - joinStartTime < threadJoinTimeout) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                joined = !m_receiveThread.joinable();
            }
            
            if (joined) {
                tempThread.join();
                getLogger()->debug("Receive thread joined successfully");
            } else {
                // If we couldn't join, detach both threads to avoid blocking
                getLogger()->warning("Receive thread join timed out after {} seconds, detaching",
                              threadJoinTimeout.count());
                tempThread.detach();
                // We can't safely join the original thread now, so we detach it
                if (m_receiveThread.joinable()) {
                    m_receiveThread.detach();
                }
            }
        }
    }

    getLogger()->info("Socket manager stopped");
}

bool DHTSocketManager::isRunning() const {
    return m_running;
}

bool DHTSocketManager::sendMessage(const uint8_t* data, size_t size, const network::EndPoint& endpoint, 
                                 std::function<void(bool)> callback) {
    if (!m_running) {
        getLogger()->error("Cannot send message: Socket manager not running");
        if (callback) {
            callback(false);
        }
        return false;
    }

    // Get a local copy of the socket pointer to avoid race conditions
    network::Socket* localSocket = nullptr;
    {
        // Use a scope to minimize the time we hold the lock
        std::lock_guard<util::CheckedMutex> lock(m_socketMutex);
        if (m_socket) {
            // Just get a raw pointer - we'll only use it while checking if it's valid
            localSocket = m_socket.get();
        }
    }

    // Check if socket is valid
    if (!localSocket || !localSocket->isValid()) {
        getLogger()->error("Cannot send message: Socket is invalid");
        if (callback) {
            callback(false);
        }
        return false;
    }

    // Send message
    int result;
    {
        // Lock again for the actual socket operation
        std::lock_guard<util::CheckedMutex> lock(m_socketMutex);
        // Check again if socket is still valid after acquiring the lock
        if (!m_socket || !m_socket->isValid()) {
            getLogger()->error("Cannot send message: Socket became invalid");
            if (callback) {
                callback(false);
            }
            return false;
        }
        result = m_socket->sendTo(data, size, endpoint);
    }

    if (result < 0) {
        getLogger()->error("Failed to send message to {}: {}",
                     endpoint.toString(),
                     network::Socket::getErrorString(m_socket->getLastError()));
        if (callback) {
            callback(false);
        }
        return false;
    }

    if (callback) {
        callback(true);
    }
    return true;
}

uint16_t DHTSocketManager::getPort() const {
    return m_port;
}

void DHTSocketManager::receiveMessages() {
    // Buffer for receiving messages
    std::vector<uint8_t> buffer(65536);

    while (m_running) {
        // Get a local copy of the socket pointer to avoid race conditions
        network::Socket* localSocket = nullptr;
        {
            // Use a scope to minimize the time we hold the lock
            std::lock_guard<util::CheckedMutex> lock(m_socketMutex);
            if (m_socket) {
                // Just get a raw pointer - we'll only use it while checking if it's valid
                localSocket = m_socket.get();
            }
        }

        // Check if socket is valid
        if (!localSocket || !localSocket->isValid()) {
            getLogger()->debug("Socket closed or invalid, exiting receive thread");
            break;
        }

        // Receive message
        network::EndPoint sender;
        int result;
        {
            // Lock again for the actual socket operation
            std::lock_guard<util::CheckedMutex> lock(m_socketMutex);
            // Check again if socket is still valid after acquiring the lock
            if (!m_socket || !m_socket->isValid()) {
                getLogger()->debug("Socket became invalid, exiting receive thread");
                break;
            }
            result = dynamic_cast<network::UDPSocketImpl*>(m_socket.get())->receiveFrom(buffer.data(), buffer.size(), sender);
        }

        // Check if we should exit the thread
        if (!m_running) {
            getLogger()->debug("Thread signaled to stop, exiting receive thread");
            break;
        }

        if (result < 0) {
            // Check if it's a would-block error
            if (m_socket->getLastError() == network::SocketError::WouldBlock) {
                // No data available, sleep for a short time
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // Check if socket was closed
            if (m_socket->getLastError() == network::SocketError::NotConnected ||
                m_socket->getLastError() == network::SocketError::ConnectionRefused ||
                m_socket->getLastError() == network::SocketError::ConnectionClosed) {
                getLogger()->debug("Socket closed, exiting receive thread");
                break;
            }

            getLogger()->error("Failed to receive message: {}",
                         network::Socket::getErrorString(m_socket->getLastError()));
            continue;
        }
        
        if (result == 0) {
            // No data received
            continue;
        }

        // Call the message callback
        if (m_messageCallback) {
            try {
                m_messageCallback(buffer.data(), static_cast<size_t>(result), sender);
            } catch (const std::exception& e) {
                getLogger()->error("Exception in message callback: {}", e.what());
            } catch (...) {
                getLogger()->error("Unknown exception in message callback");
            }
        }
    }

    getLogger()->debug("Receive thread exiting");
}

} // namespace dht_hunter::dht
