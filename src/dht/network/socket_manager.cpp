#include "dht_hunter/dht/network/socket_manager.hpp"

namespace dht_hunter::dht {

// Initialize static members
std::shared_ptr<SocketManager> SocketManager::s_instance = nullptr;
std::mutex SocketManager::s_instanceMutex;

std::shared_ptr<SocketManager> SocketManager::getInstance(const DHTConfig& config) {
    std::lock_guard<std::mutex> lock(s_instanceMutex);

    if (!s_instance) {
        s_instance = std::shared_ptr<SocketManager>(new SocketManager(config));
    }

    return s_instance;
}

SocketManager::SocketManager(const DHTConfig& config)
    : m_config(config), m_port(config.getPort()), m_running(false) {
}

SocketManager::~SocketManager() {
    stop();

    // Clear the singleton instance
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (s_instance.get() == this) {
        s_instance.reset();
    }
}

bool SocketManager::start(std::function<void(const uint8_t*, size_t, const network::EndPoint&)> receiveCallback) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running) {
        unified_event::logDebug("DHT.SocketManager", "Socket manager already running");
        return true;
    }

    // Create the socket
    m_socket = std::make_unique<network::UDPSocket>();

    // Bind the socket to the port
    if (!m_socket->bind(m_port)) {
        unified_event::logError("DHT.SocketManager", "Failed to bind socket to port " + std::to_string(m_port));
        return false;
    }

    // Set the receive callback
    m_socket->setReceiveCallback([receiveCallback](const std::vector<uint8_t>& data, const std::string& address, uint16_t port) {
        // Create an endpoint
        network::NetworkAddress networkAddress(address);
        network::EndPoint endpoint(networkAddress, port);

        // Call the callback
        receiveCallback(data.data(), data.size(), endpoint);
    });

    // Start the receive loop
    if (!m_socket->startReceiveLoop()) {
        unified_event::logError("DHT.SocketManager", "Failed to start receive loop");
        return false;
    }

    m_running = true;

    // Log that the socket manager has started
    unified_event::logInfo("DHT.SocketManager", "Socket manager started on port " + std::to_string(m_port));

    return true;
}

void SocketManager::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running) {
        unified_event::logDebug("DHT.SocketManager", "Socket manager not running");
        return;
    }

    // Stop the receive loop
    if (m_socket) {
        m_socket->stopReceiveLoop();
        m_socket->close();
    }

    m_running = false;

    // Log that the socket manager has stopped
    unified_event::logInfo("DHT.SocketManager", "Socket manager stopped");
}

bool SocketManager::isRunning() const {
    return m_running;
}

uint16_t SocketManager::getPort() const {
    return m_port;
}

ssize_t SocketManager::sendTo(const void* data, size_t size, const network::EndPoint& endpoint) {
    if (!m_running || !m_socket) {
        unified_event::logError("DHT.SocketManager", "Cannot send data: Socket manager not running or socket not available");
        return -1;
    }

    return m_socket->sendTo(data, size, endpoint.getAddress().toString(), endpoint.getPort());
}

} // namespace dht_hunter::dht
