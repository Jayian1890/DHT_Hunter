#include "dht_hunter/metadata/metadata_fetcher.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/util/hash.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>

namespace dht_hunter::metadata {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Metadata.Fetcher");
    
    // Convert info hash to hex string
    std::string infoHashToString(const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash) {
        return util::bytesToHex(infoHash.data(), infoHash.size());
    }
}

MetadataFetcher::MetadataFetcher(const MetadataFetcherConfig& config)
    : m_config(config) {
    
    // Create I/O multiplexer
    m_multiplexer = network::IOMultiplexer::create();
    
    // Get or create the connection pool
    m_connectionPool = network::ConnectionPoolManager::getInstance().getPool(m_config.connectionPoolName);
    
    // Configure the connection pool
    network::ConnectionPoolConfig poolConfig;
    poolConfig.maxConnections = m_config.maxConcurrentFetches * m_config.maxConnectionsPerInfoHash;
    poolConfig.maxConnectionsPerHost = m_config.maxConnectionsPerInfoHash;
    poolConfig.connectionTimeout = m_config.connectionTimeout;
    poolConfig.reuseConnections = m_config.reuseConnections;
    
    m_connectionPool->setConfig(poolConfig);
    
    logger->debug("Created MetadataFetcher with max concurrent fetches: {}, max connections per info hash: {}",
                 m_config.maxConcurrentFetches, m_config.maxConnectionsPerInfoHash);
}

MetadataFetcher::~MetadataFetcher() {
    stop();
}

bool MetadataFetcher::start() {
    if (m_running) {
        logger->warning("MetadataFetcher already running");
        return false;
    }
    
    // Start the connection pool maintenance
    network::ConnectionPoolManager::getInstance().startMaintenance();
    
    m_running = true;
    
    // Start threads
    m_fetchQueueThread = std::thread(&MetadataFetcher::fetchQueueThread, this);
    m_timeoutThread = std::thread(&MetadataFetcher::timeoutThread, this);
    
    logger->debug("MetadataFetcher started");
    return true;
}

void MetadataFetcher::stop() {
    if (!m_running) {
        return;
    }
    
    logger->debug("Stopping MetadataFetcher");
    
    m_running = false;
    
    // Notify the fetch queue thread
    m_queueCondition.notify_all();
    
    // Wait for threads to finish
    if (m_fetchQueueThread.joinable()) {
        m_fetchQueueThread.join();
    }
    
    if (m_timeoutThread.joinable()) {
        m_timeoutThread.join();
    }
    
    // Cancel all active fetches
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (auto& [infoHashStr, request] : m_activeFetches) {
            // Mark as completed to prevent callbacks
            request->completed = true;
            
            // Disconnect all connections
            for (auto& connection : request->connections) {
                connection->disconnect();
            }
        }
        
        // Clear active fetches and queue
        m_activeFetches.clear();
        
        while (!m_fetchQueue.empty()) {
            m_fetchQueue.pop();
        }
    }
    
    logger->debug("MetadataFetcher stopped");
}

bool MetadataFetcher::isRunning() const {
    return m_running;
}

bool MetadataFetcher::fetchMetadata(
    const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash,
    const std::vector<network::EndPoint>& endpoints,
    MetadataFetchCallback callback) {
    
    if (!m_running) {
        logger->error("Cannot fetch metadata: MetadataFetcher not running");
        return false;
    }
    
    if (endpoints.empty()) {
        logger->error("Cannot fetch metadata: No endpoints provided");
        return false;
    }
    
    std::string infoHashStr = infoHashToString(infoHash);
    
    // Check if we're already fetching this info hash
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_activeFetches.find(infoHashStr) != m_activeFetches.end()) {
            logger->warning("Already fetching metadata for info hash: {}", infoHashStr);
            return false;
        }
    }
    
    // Create a new fetch request
    auto request = std::make_shared<FetchRequest>();
    request->infoHash = infoHash;
    request->endpoints = endpoints;
    request->callback = callback;
    request->startTime = std::chrono::steady_clock::now();
    
    // Add to the queue
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fetchQueue.push(request);
    }
    
    // Notify the fetch queue thread
    m_queueCondition.notify_one();
    
    logger->debug("Queued metadata fetch for info hash: {}", infoHashStr);
    return true;
}

bool MetadataFetcher::cancelFetch(const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash) {
    if (!m_running) {
        return false;
    }
    
    std::string infoHashStr = infoHashToString(infoHash);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if it's in the active fetches
    auto it = m_activeFetches.find(infoHashStr);
    if (it != m_activeFetches.end()) {
        auto request = it->second;
        
        // Mark as completed to prevent callbacks
        request->completed = true;
        
        // Disconnect all connections
        for (auto& connection : request->connections) {
            connection->disconnect();
        }
        
        // Remove from active fetches
        m_activeFetches.erase(it);
        
        logger->debug("Canceled active metadata fetch for info hash: {}", infoHashStr);
        return true;
    }
    
    // Check if it's in the queue
    bool found = false;
    std::queue<std::shared_ptr<FetchRequest>> newQueue;
    
    while (!m_fetchQueue.empty()) {
        auto request = m_fetchQueue.front();
        m_fetchQueue.pop();
        
        if (infoHashToString(request->infoHash) == infoHashStr) {
            found = true;
        } else {
            newQueue.push(request);
        }
    }
    
    // Restore the queue without the canceled request
    m_fetchQueue = std::move(newQueue);
    
    if (found) {
        logger->debug("Canceled queued metadata fetch for info hash: {}", infoHashStr);
    }
    
    return found;
}

size_t MetadataFetcher::getActiveFetchCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeFetches.size();
}

size_t MetadataFetcher::getQueuedFetchCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_fetchQueue.size();
}

void MetadataFetcher::setConfig(const MetadataFetcherConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_config = config;
    
    // Update the connection pool configuration
    network::ConnectionPoolConfig poolConfig;
    poolConfig.maxConnections = m_config.maxConcurrentFetches * m_config.maxConnectionsPerInfoHash;
    poolConfig.maxConnectionsPerHost = m_config.maxConnectionsPerInfoHash;
    poolConfig.connectionTimeout = m_config.connectionTimeout;
    poolConfig.reuseConnections = m_config.reuseConnections;
    
    m_connectionPool->setConfig(poolConfig);
    
    logger->debug("Updated MetadataFetcher configuration: max concurrent fetches: {}, max connections per info hash: {}",
                 m_config.maxConcurrentFetches, m_config.maxConnectionsPerInfoHash);
}

MetadataFetcherConfig MetadataFetcher::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void MetadataFetcher::processFetchQueue() {
    std::unique_lock<std::mutex> lock(m_mutex);
    
    // Check if we can start more fetches
    while (m_activeFetches.size() < m_config.maxConcurrentFetches && !m_fetchQueue.empty()) {
        // Get the next request
        auto request = m_fetchQueue.front();
        m_fetchQueue.pop();
        
        std::string infoHashStr = infoHashToString(request->infoHash);
        
        // Add to active fetches
        m_activeFetches[infoHashStr] = request;
        
        // Unlock while starting connections
        lock.unlock();
        
        logger->debug("Starting metadata fetch for info hash: {}", infoHashStr);
        
        // Start connections to the endpoints
        for (size_t i = 0; i < std::min(request->endpoints.size(), static_cast<size_t>(m_config.maxConnectionsPerInfoHash)); ++i) {
            const auto& endpoint = request->endpoints[i];
            
            // Get a connection from the pool
            auto pooledConnection = m_connectionPool->getConnection(endpoint, std::chrono::milliseconds(m_config.connectionTimeout));
            
            if (pooledConnection) {
                // Create an async socket
                auto asyncSocket = std::make_unique<network::AsyncSocket>(pooledConnection->getSocket(), m_multiplexer);
                
                // Create a BT connection
                auto connection = std::make_shared<BTConnection>(
                    std::move(asyncSocket),
                    request->infoHash,
                    [this, infoHash = request->infoHash](BTConnectionState state, const std::string& message) {
                        this->handleConnectionStateChange(infoHash, nullptr, state, message);
                    },
                    [this, infoHash = request->infoHash](const std::vector<uint8_t>& metadata, uint32_t size) {
                        this->handleMetadata(infoHash, nullptr, metadata, size);
                    }
                );
                
                // Store the connection
                request->connections.push_back(connection);
                request->activeConnections++;
                
                // Connect
                if (!connection->connect(endpoint)) {
                    logger->error("Failed to connect to {}: {}", endpoint.toString(), "Connection failed");
                    request->activeConnections--;
                    continue;
                }
                
                logger->debug("Started connection to {} for info hash: {}", endpoint.toString(), infoHashStr);
            } else {
                logger->error("Failed to get connection from pool for {}", endpoint.toString());
            }
        }
        
        // Lock again for the next iteration
        lock.lock();
    }
}

void MetadataFetcher::checkTimeouts() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto now = std::chrono::steady_clock::now();
    
    // Check for timed out fetches
    for (auto it = m_activeFetches.begin(); it != m_activeFetches.end();) {
        auto& request = it->second;
        
        // Check if the fetch has timed out
        if (now - request->startTime > m_config.fetchTimeout) {
            logger->debug("Metadata fetch timed out for info hash: {}", it->first);
            
            // Mark as completed to prevent callbacks
            request->completed = true;
            
            // Disconnect all connections
            for (auto& connection : request->connections) {
                connection->disconnect();
            }
            
            // Call the callback with failure
            if (request->callback) {
                request->callback(request->infoHash, {}, 0, false);
            }
            
            // Remove from active fetches
            it = m_activeFetches.erase(it);
        } else {
            ++it;
        }
    }
}

void MetadataFetcher::fetchQueueThread() {
    logger->debug("Fetch queue thread started");
    
    while (m_running) {
        // Process the fetch queue
        processFetchQueue();
        
        // Wait for a new request or a timeout
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queueCondition.wait_for(lock, std::chrono::seconds(1), [this] {
            return !m_running || !m_fetchQueue.empty();
        });
    }
    
    logger->debug("Fetch queue thread stopped");
}

void MetadataFetcher::timeoutThread() {
    logger->debug("Timeout thread started");
    
    while (m_running) {
        // Check for timeouts
        checkTimeouts();
        
        // Sleep for a while
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    logger->debug("Timeout thread stopped");
}

void MetadataFetcher::handleConnectionStateChange(
    const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash,
    std::shared_ptr<BTConnection> connection,
    BTConnectionState state,
    const std::string& message) {
    
    std::string infoHashStr = infoHashToString(infoHash);
    
    // Find the fetch request
    std::shared_ptr<FetchRequest> request;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_activeFetches.find(infoHashStr);
        if (it == m_activeFetches.end()) {
            return;
        }
        
        request = it->second;
    }
    
    // Check if the fetch has already completed
    if (request->completed) {
        return;
    }
    
    // Handle the state change
    switch (state) {
        case BTConnectionState::Connected:
            logger->debug("Connection established for info hash: {}", infoHashStr);
            break;
            
        case BTConnectionState::Disconnected:
        case BTConnectionState::Error:
            logger->debug("Connection ended for info hash: {}: {}", infoHashStr, message);
            
            // Decrement the active connection count
            request->activeConnections--;
            
            // Check if all connections have ended
            if (request->activeConnections == 0) {
                // Complete the fetch with failure
                completeFetch(infoHash, {}, 0, false);
            }
            break;
            
        default:
            // Other states are not interesting for us
            break;
    }
}

void MetadataFetcher::handleMetadata(
    const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash,
    std::shared_ptr<BTConnection> connection,
    const std::vector<uint8_t>& metadata,
    uint32_t size) {
    
    // Complete the fetch with success
    completeFetch(infoHash, metadata, size, true);
}

void MetadataFetcher::completeFetch(
    const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash,
    const std::vector<uint8_t>& metadata,
    uint32_t size,
    bool success) {
    
    std::string infoHashStr = infoHashToString(infoHash);
    
    // Find the fetch request
    std::shared_ptr<FetchRequest> request;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_activeFetches.find(infoHashStr);
        if (it == m_activeFetches.end()) {
            return;
        }
        
        request = it->second;
        
        // Check if the fetch has already completed
        if (request->completed) {
            return;
        }
        
        // Mark as completed
        request->completed = true;
        
        // Remove from active fetches
        m_activeFetches.erase(it);
    }
    
    logger->debug("Metadata fetch completed for info hash: {}, success: {}", infoHashStr, success);
    
    // Disconnect all connections
    for (auto& connection : request->connections) {
        connection->disconnect();
    }
    
    // Call the callback
    if (request->callback) {
        request->callback(infoHash, metadata, size, success);
    }
    
    // Process the fetch queue
    processFetchQueue();
}

} // namespace dht_hunter::metadata
