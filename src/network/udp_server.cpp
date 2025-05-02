#include "dht_hunter/network/udp_server.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"

namespace dht_hunter::network {

UDPServer::UDPServer() : m_running(false) {
}

UDPServer::~UDPServer() {
    stop();
}

bool UDPServer::start(uint16_t port) {
    unified_event::logDebug("Network.UDPServer", "Starting UDP server on port " + std::to_string(port));

    if (m_running) {
        unified_event::logDebug("Network.UDPServer", "UDP server already running");
        return true;
    }

    if (!m_socket.isValid()) {
        unified_event::logError("Network.UDPServer", "Socket is invalid");
        return false;
    }

    if (!m_socket.bind(port)) {
        unified_event::logError("Network.UDPServer", "Failed to bind to port " + std::to_string(port));
        return false;
    }

    if (!m_socket.startReceiveLoop()) {
        unified_event::logError("Network.UDPServer", "Failed to start receive loop");
        return false;
    }

    m_running = true;
    unified_event::logInfo("Network.UDPServer", "UDP server started on port " + std::to_string(port));
    return true;
}

bool UDPServer::start() {
    // Get the port from the configuration
    auto configManager = utility::config::ConfigurationManager::getInstance();
    uint16_t port = static_cast<uint16_t>(configManager->getInt("dht.port", 6881));

    return start(port);
}

void UDPServer::stop() {
    unified_event::logDebug("Network.UDPServer", "Stopping UDP server");

    if (!m_running) {
        unified_event::logDebug("Network.UDPServer", "UDP server not running");
        return;
    }

    m_socket.stopReceiveLoop();
    m_socket.close();
    m_running = false;
    unified_event::logInfo("Network.UDPServer", "UDP server stopped");
}

bool UDPServer::isRunning() const {
    return m_running;
}

ssize_t UDPServer::sendTo(const std::vector<uint8_t>& data, const std::string& address, uint16_t port) {
    if (!m_running) {
        unified_event::logWarning("Network.UDPServer", "Cannot send data - server not running");
        return -1;
    }

    ssize_t result = m_socket.sendTo(data.data(), data.size(), address, port);
    if (result < 0) {
        unified_event::logWarning("Network.UDPServer", "Failed to send data to " + address + ":" + std::to_string(port));
    }

    return result;
}

void UDPServer::setMessageHandler(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback) {
    m_socket.setReceiveCallback(std::move(callback));
}

} // namespace dht_hunter::network
