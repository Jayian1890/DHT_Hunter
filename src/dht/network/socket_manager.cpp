#include "dht_hunter/dht/network/socket_manager.hpp"

namespace dht_hunter::dht {

SocketManager::SocketManager(const DHTConfig& config)
    : m_config(config), m_port(config.getPort()), m_running(false),
      m_logger(event::Logger::forComponent("DHT.SocketManager")) {
    m_logger.info("Creating socket manager for port {}", m_port);
}

SocketManager::~SocketManager() {
    stop();
}

bool SocketManager::start(std::function<void(const uint8_t*, size_t, const network::EndPoint&)> receiveCallback) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running) {
        m_logger.warning("Socket manager already running");
        return true;
    }

    // Create the socket
    m_socket = std::make_unique<network::UDPSocket>();

    // Bind the socket to the port
    if (!m_socket->bind(m_port)) {
        m_logger.error("Failed to bind socket to port {}", m_port);
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
        m_logger.error("Failed to start receive loop");
        return false;
    }

    m_running = true;
    m_logger.info("Socket manager started on port {}", m_port);
    return true;
}

void SocketManager::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running) {
        return;
    }

    // Stop the receive loop
    if (m_socket) {
        m_socket->stopReceiveLoop();
        m_socket->close();
    }

    m_running = false;
    m_logger.info("Socket manager stopped");
}

bool SocketManager::isRunning() const {
    return m_running;
}

uint16_t SocketManager::getPort() const {
    return m_port;
}

ssize_t SocketManager::sendTo(const void* data, size_t size, const network::EndPoint& endpoint) {
    if (!m_running || !m_socket) {
        m_logger.error("Cannot send: Socket manager not running");
        return -1;
    }

    return m_socket->sendTo(data, size, endpoint.getAddress().toString(), endpoint.getPort());
}

} // namespace dht_hunter::dht
