#include "dht_hunter/network/udp_server.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"

namespace dht_hunter::network {

UDPServer::UDPServer() : m_running(false) {
}

UDPServer::~UDPServer() {
    stop();
}

bool UDPServer::start(uint16_t port) {    // Logger initialization removed

    if (m_running) {
        return true;
    }

    if (!m_socket.isValid()) {
        return false;
    }

    if (!m_socket.bind(port)) {
        return false;
    }

    if (!m_socket.startReceiveLoop()) {
        return false;
    }

    m_running = true;
    return true;
}

void UDPServer::stop() {    // Logger initialization removed

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

ssize_t UDPServer::sendTo(const std::vector<uint8_t>& data, const std::string& address, uint16_t port) {    // Logger initialization removed

    if (!m_running) {
        return -1;
    }

    return m_socket.sendTo(data.data(), data.size(), address, port);
}

void UDPServer::setMessageHandler(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback) {
    m_socket.setReceiveCallback(std::move(callback));
}

} // namespace dht_hunter::network
