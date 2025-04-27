#include "dht_hunter/network/udp_server.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"

namespace dht_hunter::network {

UDPServer::UDPServer() : m_running(false) {
}

UDPServer::~UDPServer() {
    stop();
}

bool UDPServer::start(uint16_t port) {
    auto logger = event::Logger::forComponent("Network.UDPServer");

    if (m_running) {
        logger.warning("Server is already running");
        return true;
    }

    if (!m_socket.isValid()) {
        logger.error("Socket is invalid");
        return false;
    }

    if (!m_socket.bind(port)) {
        logger.error("Failed to bind socket to port {}", port);
        return false;
    }

    if (!m_socket.startReceiveLoop()) {
        logger.error("Failed to start receive loop");
        return false;
    }

    m_running = true;
    logger.info("UDP server started on port {}", port);
    return true;
}

void UDPServer::stop() {
    auto logger = event::Logger::forComponent("Network.UDPServer");

    if (!m_running) {
        logger.warning("Server is not running");
        return;
    }

    m_socket.stopReceiveLoop();
    m_socket.close();
    m_running = false;

    logger.debug("UDP server stopped");
}

bool UDPServer::isRunning() const {
    return m_running;
}

ssize_t UDPServer::sendTo(const std::vector<uint8_t>& data, const std::string& address, uint16_t port) {
    auto logger = event::Logger::forComponent("Network.UDPServer");

    if (!m_running) {
        logger.error("Cannot send: Server is not running");
        return -1;
    }

    return m_socket.sendTo(data.data(), data.size(), address, port);
}

void UDPServer::setMessageHandler(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback) {
    m_socket.setReceiveCallback(std::move(callback));
}

} // namespace dht_hunter::network
