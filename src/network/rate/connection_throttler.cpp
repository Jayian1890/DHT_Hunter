#include "dht_hunter/network/rate/connection_throttler.hpp"
#include <algorithm>

namespace dht_hunter::network {

ConnectionThrottler::ConnectionThrottler(size_t maxConnectionsPerEndpoint, std::chrono::seconds connectionTimeout)
    : m_maxConnectionsPerEndpoint(maxConnectionsPerEndpoint), m_connectionTimeout(connectionTimeout) {
}

ConnectionThrottler::~ConnectionThrottler() {
}

bool ConnectionThrottler::tryAcquire(const EndPoint& endpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Remove expired connections
    removeExpiredConnections();
    
    // Check if we've reached the maximum number of connections for this endpoint
    auto& connections = m_connections[endpoint];
    if (connections.size() >= m_maxConnectionsPerEndpoint) {
        return false;
    }
    
    // Add a new connection
    Connection connection;
    connection.timestamp = std::chrono::steady_clock::now();
    connections.push_back(connection);
    
    return true;
}

void ConnectionThrottler::release(const EndPoint& endpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Remove the oldest connection for this endpoint
    auto it = m_connections.find(endpoint);
    if (it != m_connections.end() && !it->second.empty()) {
        it->second.erase(it->second.begin());
    }
}

size_t ConnectionThrottler::getMaxConnectionsPerEndpoint() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxConnectionsPerEndpoint;
}

void ConnectionThrottler::setMaxConnectionsPerEndpoint(size_t maxConnectionsPerEndpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxConnectionsPerEndpoint = maxConnectionsPerEndpoint;
}

std::chrono::seconds ConnectionThrottler::getConnectionTimeout() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connectionTimeout;
}

void ConnectionThrottler::setConnectionTimeout(std::chrono::seconds connectionTimeout) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connectionTimeout = connectionTimeout;
}

size_t ConnectionThrottler::getActiveConnections(const EndPoint& endpoint) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_connections.find(endpoint);
    if (it != m_connections.end()) {
        return it->second.size();
    }
    
    return 0;
}

size_t ConnectionThrottler::getTotalActiveConnections() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t total = 0;
    for (const auto& pair : m_connections) {
        total += pair.second.size();
    }
    
    return total;
}

void ConnectionThrottler::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connections.clear();
}

void ConnectionThrottler::removeExpiredConnections() {
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = m_connections.begin(); it != m_connections.end();) {
        auto& connections = it->second;
        
        // Remove expired connections
        connections.erase(
            std::remove_if(connections.begin(), connections.end(),
                [&](const Connection& connection) {
                    return now - connection.timestamp > m_connectionTimeout;
                }),
            connections.end());
        
        // Remove empty endpoints
        if (connections.empty()) {
            it = m_connections.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace dht_hunter::network
