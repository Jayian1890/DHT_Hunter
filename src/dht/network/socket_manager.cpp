#include "dht_hunter/dht/network/socket_manager.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"

namespace dht_hunter::dht {

// Initialize static members
std::shared_ptr<SocketManager> SocketManager::s_instance = nullptr;
std::recursive_mutex SocketManager::s_instanceMutex;

std::shared_ptr<SocketManager> SocketManager::getInstance(const DHTConfig& config) {
    try {
        return utility::thread::withLock(s_instanceMutex, [&]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<SocketManager>(new SocketManager(config));
            }
            return s_instance;
        }, "SocketManager::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.SocketManager", e.what());
        return nullptr;
    }
}

SocketManager::SocketManager(const DHTConfig& config)
    : m_config(config), m_port(config.getPort()), m_running(false) {
}

SocketManager::~SocketManager() {
    stop();

    // Clear the singleton instance
    try {
        utility::thread::withLock(s_instanceMutex, [this]() {
            if (s_instance.get() == this) {
                s_instance.reset();
            }
        }, "SocketManager::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.SocketManager", e.what());
    }
}

bool SocketManager::start(std::function<void(const uint8_t*, size_t, const network::EndPoint&)> receiveCallback) {
    try {
        return utility::thread::withLock(m_mutex, [this, receiveCallback]() {
            if (m_running) {
                return true;
            }

            // Create the socket
            m_socket = std::make_unique<network::UDPSocket>();

            // Bind the socket to the port
            if (!m_socket->bind(m_port)) {
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
                return false;
            }

            m_running = true;

            // Socket manager started event is published through the event system

            return true;
        }, "SocketManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.SocketManager", e.what());
        return false;
    }
}

void SocketManager::stop() {
    try {
        utility::thread::withLock(m_mutex, [this]() {
            if (!m_running) {
                return;
            }

            // Stop the receive loop
            if (m_socket) {
                m_socket->stopReceiveLoop();
                m_socket->close();
            }

            m_running = false;

            // Socket manager stopped event is published through the event system
        }, "SocketManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.SocketManager", e.what());
    }
}

bool SocketManager::isRunning() const {
    return m_running;
}

uint16_t SocketManager::getPort() const {
    return m_port;
}

ssize_t SocketManager::sendTo(const void* data, size_t size, const network::EndPoint& endpoint) {
    if (!m_running || !m_socket) {
        return -1;
    }

    return m_socket->sendTo(data, size, endpoint.getAddress().toString(), endpoint.getPort());
}

} // namespace dht_hunter::dht
