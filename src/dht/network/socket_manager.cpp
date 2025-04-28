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
    unified_event::logTrace("DHT.SocketManager", "TRACE: start() entry");
    try {
        unified_event::logTrace("DHT.SocketManager", "TRACE: Acquiring mutex");
        return utility::thread::withLock(m_mutex, [this, receiveCallback]() {
            unified_event::logTrace("DHT.SocketManager", "TRACE: Mutex acquired");
            if (m_running) {
                unified_event::logTrace("DHT.SocketManager", "TRACE: Already running, returning true");
                return true;
            }

            // Create the socket
            unified_event::logTrace("DHT.SocketManager", "TRACE: Creating UDPSocket");
            m_socket = std::make_unique<network::UDPSocket>();
            unified_event::logTrace("DHT.SocketManager", "TRACE: UDPSocket created");

            // Try to bind the socket to the port
            // If the default port is already in use, try the next 10 ports
            bool bound = false;
            uint16_t port = m_port;
            for (int i = 0; i < 10; i++) {
                unified_event::logTrace("DHT.SocketManager", "TRACE: Trying to bind socket to port " + std::to_string(port));
                if (m_socket->bind(port)) {
                    unified_event::logTrace("DHT.SocketManager", "TRACE: Socket bound to port " + std::to_string(port));
                    m_port = port; // Update the port if we bound to a different one
                    bound = true;
                    break;
                }
                unified_event::logTrace("DHT.SocketManager", "TRACE: Failed to bind socket to port " + std::to_string(port));
                port++;
            }

            if (!bound) {
                unified_event::logTrace("DHT.SocketManager", "TRACE: Failed to bind socket to any port");
                return false;
            }

            // Set the receive callback
            unified_event::logTrace("DHT.SocketManager", "TRACE: Setting receive callback");
            m_socket->setReceiveCallback([receiveCallback](const std::vector<uint8_t>& data, const std::string& address, uint16_t port) {
                unified_event::logTrace("DHT.SocketManager", "TRACE: Received data from " + address + ":" + std::to_string(port) + ", size: " + std::to_string(data.size()));
                // Create an endpoint
                network::NetworkAddress networkAddress(address);
                network::EndPoint endpoint(networkAddress, port);

                // Call the callback
                unified_event::logTrace("DHT.SocketManager", "TRACE: Calling receive callback");
                receiveCallback(data.data(), data.size(), endpoint);
                unified_event::logTrace("DHT.SocketManager", "TRACE: Receive callback completed");
            });
            unified_event::logTrace("DHT.SocketManager", "TRACE: Receive callback set");

            // Start the receive loop
            unified_event::logTrace("DHT.SocketManager", "TRACE: Starting receive loop");
            if (!m_socket->startReceiveLoop()) {
                unified_event::logTrace("DHT.SocketManager", "TRACE: Failed to start receive loop");
                return false;
            }
            unified_event::logTrace("DHT.SocketManager", "TRACE: Receive loop started");

            unified_event::logTrace("DHT.SocketManager", "TRACE: Setting m_running to true");
            m_running = true;

            // Socket manager started event is published through the event system
            unified_event::logTrace("DHT.SocketManager", "TRACE: Socket manager started successfully");

            return true;
        }, "SocketManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.SocketManager", e.what());
        unified_event::logTrace("DHT.SocketManager", "TRACE: Lock timeout exception: " + std::string(e.what()));
        return false;
    } catch (const std::exception& e) {
        unified_event::logError("DHT.SocketManager", "Exception in start(): " + std::string(e.what()));
        unified_event::logTrace("DHT.SocketManager", "TRACE: Exception in start(): " + std::string(e.what()));
        return false;
    } catch (...) {
        unified_event::logError("DHT.SocketManager", "Unknown exception in start()");
        unified_event::logTrace("DHT.SocketManager", "TRACE: Unknown exception in start()");
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
