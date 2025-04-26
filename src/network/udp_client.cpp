#include "dht_hunter/network/udp_client.hpp"
#include "dht_hunter/logforge/logforge.hpp"

namespace dht_hunter::network {

UDPClient::UDPClient() : m_running(false) {
}

UDPClient::~UDPClient() {
    stop();
}

bool UDPClient::start() {
    auto logger = logforge::LogForge::getInstance().getLogger("Network.UDPClient");
    
    if (m_running) {
        logger->warning("Client is already running");
        return true;
    }
    
    if (!m_socket.isValid()) {
        logger->error("Socket is invalid");
        return false;
    }
    
    // Bind to a random port
    if (!m_socket.bind(0)) {
        logger->error("Failed to bind socket");
        return false;
    }
    
    if (!m_socket.startReceiveLoop()) {
        logger->error("Failed to start receive loop");
        return false;
    }
    
    m_running = true;
    logger->info("UDP client started");
    return true;
}

void UDPClient::stop() {
    auto logger = logforge::LogForge::getInstance().getLogger("Network.UDPClient");
    
    if (!m_running) {
        logger->warning("Client is not running");
        return;
    }
    
    m_socket.stopReceiveLoop();
    m_socket.close();
    m_running = false;
    
    logger->info("UDP client stopped");
}

bool UDPClient::isRunning() const {
    return m_running;
}

int UDPClient::send(const std::vector<uint8_t>& data, const std::string& address, uint16_t port) {
    auto logger = logforge::LogForge::getInstance().getLogger("Network.UDPClient");
    
    if (!m_running) {
        logger->error("Cannot send: Client is not running");
        return -1;
    }
    
    return m_socket.sendTo(data.data(), data.size(), address, port);
}

void UDPClient::setMessageHandler(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback) {
    m_socket.setReceiveCallback(std::move(callback));
}

} // namespace dht_hunter::network
