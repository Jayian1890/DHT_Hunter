#include "dht_hunter/network/udp_client.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"

namespace dht_hunter::network {

UDPClient::UDPClient() : m_running(false) {
}

UDPClient::~UDPClient() {
    stop();
}

bool UDPClient::start() {    // Logger initialization removed

    if (m_running) {
        return true;
    }

    if (!m_socket.isValid()) {
        return false;
    }

    // Bind to a random port
    if (!m_socket.bind(0)) {
        return false;
    }

    if (!m_socket.startReceiveLoop()) {
        return false;
    }

    m_running = true;
    return true;
}

void UDPClient::stop() {    // Logger initialization removed

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

ssize_t UDPClient::send(const std::vector<uint8_t>& data, const std::string& address, uint16_t port) {    // Logger initialization removed

    if (!m_running) {
        return -1;
    }

    return m_socket.sendTo(data.data(), data.size(), address, port);
}

void UDPClient::setMessageHandler(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback) {
    m_socket.setReceiveCallback(std::move(callback));
}

} // namespace dht_hunter::network
