#include "dht_hunter/network/connection_pool.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/network/socket_factory.hpp"

namespace dht_hunter::network {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Network.ConnectionPool");
}

//
// PooledConnection implementation
//

PooledConnection::PooledConnection(std::shared_ptr<Socket> socket, const EndPoint& endpoint)
    : m_socket(socket),
      m_endpoint(endpoint),
      m_creationTime(std::chrono::steady_clock::now()),
      m_lastUsedTime(m_creationTime),
      m_isValid(true) {
    
    logger->debug("Created pooled connection to {}", endpoint.toString());
}

PooledConnection::~PooledConnection() {
    close();
}

std::shared_ptr<Socket> PooledConnection::getSocket() const {
    return m_socket;
}

EndPoint PooledConnection::getEndpoint() const {
    return m_endpoint;
}

std::chrono::steady_clock::time_point PooledConnection::getCreationTime() const {
    return m_creationTime;
}

std::chrono::steady_clock::time_point PooledConnection::getLastUsedTime() const {
    return m_lastUsedTime;
}

void PooledConnection::updateLastUsedTime() {
    m_lastUsedTime = std::chrono::steady_clock::now();
}

bool PooledConnection::isValid() const {
    return m_isValid.load() && m_socket != nullptr;
}

bool PooledConnection::validate() {
    if (!m_socket) {
        m_isValid.store(false);
        return false;
    }
    
    // Check if the socket is still connected
    // For TCP sockets, we can try to peek at the data
    uint8_t buffer[1];
    int result = m_socket->receive(buffer, sizeof(buffer), Socket::ReceiveFlags::Peek);
    
    // If the result is 0, the connection has been closed by the peer
    // If the result is negative, there was an error
    if (result == 0) {
        logger->debug("Connection to {} has been closed by the peer", m_endpoint.toString());
        m_isValid.store(false);
        return false;
    }
    
    // Check for errors
    if (result < 0) {
        auto error = m_socket->getLastError();
        
        // Some errors are expected when peeking (e.g., EWOULDBLOCK, EAGAIN)
        // These errors indicate that the socket is still valid but there's no data
        if (error == SocketError::WouldBlock || error == SocketError::Again || 
            error == SocketError::InProgress || error == SocketError::NotConnected) {
            // These errors are expected and don't indicate a problem with the connection
            return true;
        }
        
        logger->debug("Connection to {} is invalid: {}", m_endpoint.toString(), 
                     m_socket->getErrorString(error));
        m_isValid.store(false);
        return false;
    }
    
    return true;
}

void PooledConnection::close() {
    if (m_socket) {
        m_socket->close();
        m_socket.reset();
    }
    
    m_isValid.store(false);
}

//
// ConnectionPool implementation
//

ConnectionPool::ConnectionPool(const ConnectionPoolConfig& config)
    : m_config(config),
      m_totalConnections(0) {
    
    // Create a default connection throttler if none was provided
    if (!m_config.connectionThrottler) {
        m_config.connectionThrottler = std::make_shared<ConnectionThrottler>(m_config.maxConnections);
    }
    
    logger->debug("Created connection pool with max connections: {}, max connections per host: {}", 
                 m_config.maxConnections, m_config.maxConnectionsPerHost);
}

ConnectionPool::~ConnectionPool() {
    closeAll();
}

std::shared_ptr<PooledConnection> ConnectionPool::getConnection(const EndPoint& endpoint, 
                                                              std::chrono::milliseconds timeout) {
    // First, try to get an idle connection
    auto idleConnection = getIdleConnection(endpoint);
    if (idleConnection) {
        // Update the last used time
        idleConnection->updateLastUsedTime();
        
        // Validate the connection if required
        if (m_config.validateConnectionsBeforeReuse && !idleConnection->validate()) {
            logger->debug("Idle connection to {} is invalid, creating a new one", endpoint.toString());
            
            // Remove the connection from the active connections
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                
                auto& activeConnectionsForHost = m_activeConnections[endpoint.getAddress()];
                activeConnectionsForHost.erase(
                    std::remove(activeConnectionsForHost.begin(), activeConnectionsForHost.end(), idleConnection),
                    activeConnectionsForHost.end());
                
                // Decrement the total connection count
                m_totalConnections--;
            }
            
            // Create a new connection
            return createConnection(endpoint, timeout);
        }
        
        logger->debug("Reusing idle connection to {}", endpoint.toString());
        return idleConnection;
    }
    
    // If we couldn't get an idle connection, create a new one
    return createConnection(endpoint, timeout);
}

void ConnectionPool::returnConnection(std::shared_ptr<PooledConnection> connection) {
    if (!connection || !connection->isValid()) {
        // If the connection is invalid, don't return it to the pool
        if (connection) {
            // Close the connection
            connection->close();
            
            // Remove the connection from the active connections
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                
                auto& activeConnectionsForHost = m_activeConnections[connection->getEndpoint().getAddress()];
                activeConnectionsForHost.erase(
                    std::remove(activeConnectionsForHost.begin(), activeConnectionsForHost.end(), connection),
                    activeConnectionsForHost.end());
                
                // Decrement the total connection count
                m_totalConnections--;
            }
        }
        
        return;
    }
    
    // If connection reuse is disabled, close the connection
    if (!m_config.reuseConnections) {
        connection->close();
        
        // Remove the connection from the active connections
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            auto& activeConnectionsForHost = m_activeConnections[connection->getEndpoint().getAddress()];
            activeConnectionsForHost.erase(
                std::remove(activeConnectionsForHost.begin(), activeConnectionsForHost.end(), connection),
                activeConnectionsForHost.end());
            
            // Decrement the total connection count
            m_totalConnections--;
        }
        
        return;
    }
    
    // Update the last used time
    connection->updateLastUsedTime();
    
    // Add the connection to the idle connections
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_idleConnections[connection->getEndpoint().getAddress()].push(connection);
    }
    
    logger->debug("Returned connection to {} to the pool", connection->getEndpoint().toString());
}

void ConnectionPool::closeAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Close all active connections
    for (auto& [host, connections] : m_activeConnections) {
        for (auto& connection : connections) {
            connection->close();
        }
    }
    
    // Clear the active connections
    m_activeConnections.clear();
    
    // Close all idle connections
    for (auto& [host, connections] : m_idleConnections) {
        while (!connections.empty()) {
            auto connection = connections.front();
            connections.pop();
            connection->close();
        }
    }
    
    // Clear the idle connections
    m_idleConnections.clear();
    
    // Reset the total connection count
    m_totalConnections.store(0);
    
    logger->debug("Closed all connections");
}

size_t ConnectionPool::getActiveConnectionCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t count = 0;
    for (const auto& [host, connections] : m_activeConnections) {
        count += connections.size();
    }
    
    return count;
}

size_t ConnectionPool::getIdleConnectionCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t count = 0;
    for (const auto& [host, connections] : m_idleConnections) {
        count += connections.size();
    }
    
    return count;
}

size_t ConnectionPool::getConnectionCountForHost(const NetworkAddress& host) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t count = 0;
    
    // Count active connections
    auto activeIt = m_activeConnections.find(host);
    if (activeIt != m_activeConnections.end()) {
        count += activeIt->second.size();
    }
    
    // Count idle connections
    auto idleIt = m_idleConnections.find(host);
    if (idleIt != m_idleConnections.end()) {
        count += idleIt->second.size();
    }
    
    return count;
}

void ConnectionPool::setConfig(const ConnectionPoolConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_config = config;
    
    // Create a default connection throttler if none was provided
    if (!m_config.connectionThrottler) {
        m_config.connectionThrottler = std::make_shared<ConnectionThrottler>(m_config.maxConnections);
    } else {
        // Update the max connections in the throttler
        m_config.connectionThrottler->setMaxConnections(m_config.maxConnections);
    }
    
    logger->debug("Updated connection pool configuration: max connections: {}, max connections per host: {}", 
                 m_config.maxConnections, m_config.maxConnectionsPerHost);
}

ConnectionPoolConfig ConnectionPool::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void ConnectionPool::maintenance() {
    // Remove expired connections
    removeExpiredConnections();
}

std::shared_ptr<PooledConnection> ConnectionPool::createConnection(const EndPoint& endpoint, 
                                                                 std::chrono::milliseconds timeout) {
    // Check if we've reached the maximum number of connections
    if (m_totalConnections.load() >= m_config.maxConnections) {
        logger->debug("Maximum number of connections reached ({}), cannot create new connection to {}", 
                     m_config.maxConnections, endpoint.toString());
        return nullptr;
    }
    
    // Check if we've reached the maximum number of connections per host
    if (getConnectionCountForHost(endpoint.getAddress()) >= m_config.maxConnectionsPerHost) {
        logger->debug("Maximum number of connections per host reached ({}) for {}, cannot create new connection", 
                     m_config.maxConnectionsPerHost, endpoint.getAddress().toString());
        return nullptr;
    }
    
    // Request a connection slot from the throttler
    if (!m_config.connectionThrottler->requestConnection()) {
        logger->debug("Connection throttler denied connection to {}", endpoint.toString());
        return nullptr;
    }
    
    // Create a new socket
    auto socket = SocketFactory::createTCPSocket();
    if (!socket) {
        logger->error("Failed to create TCP socket");
        m_config.connectionThrottler->releaseConnection();
        return nullptr;
    }
    
    // Set the socket to non-blocking mode
    if (!socket->setNonBlocking(true)) {
        logger->error("Failed to set socket to non-blocking mode: {}", 
                     socket->getErrorString(socket->getLastError()));
        m_config.connectionThrottler->releaseConnection();
        return nullptr;
    }
    
    // Connect to the endpoint
    if (!socket->connect(endpoint)) {
        auto error = socket->getLastError();
        
        // Check if the connection is in progress
        if (error != SocketError::InProgress && error != SocketError::WouldBlock) {
            logger->error("Failed to connect to {}: {}", 
                         endpoint.toString(), socket->getErrorString(error));
            m_config.connectionThrottler->releaseConnection();
            return nullptr;
        }
        
        // Wait for the connection to complete
        auto startTime = std::chrono::steady_clock::now();
        auto endTime = startTime + timeout;
        
        while (std::chrono::steady_clock::now() < endTime) {
            // Check if the socket is writable (connection complete)
            fd_set writeSet;
            FD_ZERO(&writeSet);
            FD_SET(socket->getHandle(), &writeSet);
            
            // Set up the timeout
            struct timeval tv;
            auto remainingTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                endTime - std::chrono::steady_clock::now());
            tv.tv_sec = static_cast<long>(remainingTime.count() / 1000);
            tv.tv_usec = static_cast<long>((remainingTime.count() % 1000) * 1000);
            
            // Wait for the socket to become writable
            int result = select(static_cast<int>(socket->getHandle() + 1), nullptr, &writeSet, nullptr, &tv);
            
            if (result < 0) {
                // Error
                logger->error("Select failed: {}", socket->getErrorString(socket->getLastError()));
                m_config.connectionThrottler->releaseConnection();
                return nullptr;
            } else if (result == 0) {
                // Timeout
                continue;
            } else {
                // Socket is writable, check for errors
                int error = 0;
                socklen_t errorLen = sizeof(error);
                
                if (getsockopt(socket->getHandle(), SOL_SOCKET, SO_ERROR, 
                              reinterpret_cast<char*>(&error), &errorLen) < 0) {
                    logger->error("Failed to get socket error: {}", 
                                 socket->getErrorString(socket->getLastError()));
                    m_config.connectionThrottler->releaseConnection();
                    return nullptr;
                }
                
                if (error != 0) {
                    logger->error("Connection to {} failed: {}", 
                                 endpoint.toString(), socket->getErrorString(static_cast<SocketError>(error)));
                    m_config.connectionThrottler->releaseConnection();
                    return nullptr;
                }
                
                // Connection successful
                break;
            }
        }
        
        // Check if we timed out
        if (std::chrono::steady_clock::now() >= endTime) {
            logger->error("Connection to {} timed out", endpoint.toString());
            m_config.connectionThrottler->releaseConnection();
            return nullptr;
        }
    }
    
    // Create a pooled connection
    auto connection = std::make_shared<PooledConnection>(socket, endpoint);
    
    // Add the connection to the active connections
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_activeConnections[endpoint.getAddress()].push_back(connection);
        
        // Increment the total connection count
        m_totalConnections++;
    }
    
    logger->debug("Created new connection to {}", endpoint.toString());
    return connection;
}

std::shared_ptr<PooledConnection> ConnectionPool::getIdleConnection(const EndPoint& endpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if there are any idle connections for this host
    auto it = m_idleConnections.find(endpoint.getAddress());
    if (it == m_idleConnections.end() || it->second.empty()) {
        return nullptr;
    }
    
    // Get the first idle connection
    auto connection = it->second.front();
    it->second.pop();
    
    // Check if the connection is valid
    if (!connection->isValid()) {
        // Remove the connection from the active connections
        auto& activeConnectionsForHost = m_activeConnections[endpoint.getAddress()];
        activeConnectionsForHost.erase(
            std::remove(activeConnectionsForHost.begin(), activeConnectionsForHost.end(), connection),
            activeConnectionsForHost.end());
        
        // Decrement the total connection count
        m_totalConnections--;
        
        return nullptr;
    }
    
    return connection;
}

void ConnectionPool::removeExpiredConnections() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto now = std::chrono::steady_clock::now();
    
    // Check idle connections
    for (auto it = m_idleConnections.begin(); it != m_idleConnections.end();) {
        auto& [host, connections] = *it;
        
        // Process all idle connections for this host
        std::queue<std::shared_ptr<PooledConnection>> remainingConnections;
        
        while (!connections.empty()) {
            auto connection = connections.front();
            connections.pop();
            
            // Check if the connection has expired
            auto idleTime = std::chrono::duration_cast<std::chrono::seconds>(
                now - connection->getLastUsedTime());
            auto lifetime = std::chrono::duration_cast<std::chrono::seconds>(
                now - connection->getCreationTime());
            
            if (idleTime > m_config.connectionIdleTimeout || lifetime > m_config.connectionMaxLifetime) {
                // Connection has expired, close it
                connection->close();
                
                // Remove the connection from the active connections
                auto& activeConnectionsForHost = m_activeConnections[host];
                activeConnectionsForHost.erase(
                    std::remove(activeConnectionsForHost.begin(), activeConnectionsForHost.end(), connection),
                    activeConnectionsForHost.end());
                
                // Decrement the total connection count
                m_totalConnections--;
                
                logger->debug("Removed expired connection to {}", connection->getEndpoint().toString());
            } else {
                // Connection is still valid, keep it
                remainingConnections.push(connection);
            }
        }
        
        // Update the idle connections for this host
        if (remainingConnections.empty()) {
            it = m_idleConnections.erase(it);
        } else {
            connections = std::move(remainingConnections);
            ++it;
        }
    }
    
    // Check active connections
    for (auto it = m_activeConnections.begin(); it != m_activeConnections.end();) {
        auto& [host, connections] = *it;
        
        // Process all active connections for this host
        connections.erase(
            std::remove_if(connections.begin(), connections.end(),
                [this, now](const std::shared_ptr<PooledConnection>& connection) {
                    // Check if the connection has expired
                    auto lifetime = std::chrono::duration_cast<std::chrono::seconds>(
                        now - connection->getCreationTime());
                    
                    if (lifetime > m_config.connectionMaxLifetime) {
                        // Connection has expired, close it
                        connection->close();
                        
                        // Decrement the total connection count
                        m_totalConnections--;
                        
                        logger->debug("Removed expired connection to {}", connection->getEndpoint().toString());
                        return true;
                    }
                    
                    return false;
                }),
            connections.end());
        
        // Remove the host if there are no more connections
        if (connections.empty()) {
            it = m_activeConnections.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace dht_hunter::network
