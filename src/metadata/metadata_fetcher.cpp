#include "dht_hunter/metadata/metadata_fetcher.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include "dht_hunter/util/hash.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>

DEFINE_COMPONENT_LOGGER("Metadata", "Fetcher")

namespace dht_hunter::metadata {

namespace {

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

    // Create metadata storage if enabled
    if (m_config.useMetadataStorage) {
        m_metadataStorage = std::make_shared<storage::MetadataStorage>(m_config.metadataStorageDirectory);
    }

    getLogger()->debug("Created MetadataFetcher with max concurrent fetches: {}, max connections per info hash: {}",
                 m_config.maxConcurrentFetches, m_config.maxConnectionsPerInfoHash);
}

MetadataFetcher::~MetadataFetcher() {
    stop();
}

bool MetadataFetcher::start() {
    if (m_running) {
        getLogger()->warning("MetadataFetcher already running");
        return false;
    }

    // Start the connection pool maintenance
    network::ConnectionPoolManager::getInstance().startMaintenance();

    // The new MetadataStorage class is initialized in the constructor
    if (m_config.useMetadataStorage && m_metadataStorage) {
        // Just log the count of items
        getLogger()->info("Metadata storage initialized with {} items", m_metadataStorage->count());
    }

    m_running = true;

    // Start threads
    m_fetchQueueThread = std::thread(&MetadataFetcher::fetchQueueThread, this);
    m_timeoutThread = std::thread(&MetadataFetcher::timeoutThread, this);

    getLogger()->debug("MetadataFetcher started");
    return true;
}

void MetadataFetcher::stop() {
    if (!m_running) {
        return;
    }

    getLogger()->debug("Stopping MetadataFetcher");

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

    // The new MetadataStorage class doesn't need explicit shutdown
    if (m_config.useMetadataStorage && m_metadataStorage) {
        m_metadataStorage.reset();
        getLogger()->debug("Metadata storage released");
    }

    getLogger()->debug("MetadataFetcher stopped");
}

bool MetadataFetcher::isRunning() const {
    return m_running;
}

bool MetadataFetcher::fetchMetadata(
    const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash,
    const std::vector<network::EndPoint>& endpoints,
    MetadataFetchCallback callback) {

    if (!m_running) {
        getLogger()->error("Cannot fetch metadata: MetadataFetcher not running");
        return false;
    }

    if (endpoints.empty()) {
        getLogger()->warning("Cannot fetch metadata: No endpoints provided for info hash: {}", infoHashToString(infoHash));

        // If we have a callback, call it with failure instead of just returning false
        if (callback) {
            // Schedule the callback to be called asynchronously
            std::thread([callback, infoHash]() {
                callback(infoHash, {}, 0, false);
            }).detach();
        }

        return false;
    }

    std::string infoHashStr = infoHashToString(infoHash);

    // Check if we're already fetching this info hash
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_activeFetches.find(infoHashStr) != m_activeFetches.end()) {
            getLogger()->warning("Already fetching metadata for info hash: {}", infoHashStr);
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

    getLogger()->debug("Queued metadata fetch for info hash: {}", infoHashStr);
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

        getLogger()->debug("Canceled active metadata fetch for info hash: {}", infoHashStr);
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
        getLogger()->debug("Canceled queued metadata fetch for info hash: {}", infoHashStr);
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

    getLogger()->debug("Updated MetadataFetcher configuration: max concurrent fetches: {}, max connections per info hash: {}",
                 m_config.maxConcurrentFetches, m_config.maxConnectionsPerInfoHash);
}

MetadataFetcherConfig MetadataFetcher::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

std::shared_ptr<storage::MetadataStorage> MetadataFetcher::getMetadataStorage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metadataStorage;
}

void MetadataFetcher::setMetadataStorage(std::shared_ptr<storage::MetadataStorage> metadataStorage) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metadataStorage = metadataStorage;
    getLogger()->info("Metadata storage set in MetadataFetcher");
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

        getLogger()->debug("Starting metadata fetch for info hash: {}", infoHashStr);

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
                    },
                    m_config.peerIdPrefix,
                    m_config.clientVersion
                );

                // Store the connection
                request->connections.push_back(connection);
                request->activeConnections++;

                // Connect
                if (!connection->connect(endpoint)) {
                    getLogger()->error("Failed to connect to {}: {}", endpoint.toString(), "Connection failed");
                    request->activeConnections--;
                    continue;
                }

                getLogger()->debug("Started connection to {} for info hash: {}", endpoint.toString(), infoHashStr);
            } else {
                getLogger()->error("Failed to get connection from pool for {}", endpoint.toString());
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
            getLogger()->debug("Metadata fetch timed out for info hash: {}", it->first);

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
    getLogger()->debug("Fetch queue thread started");

    while (m_running) {
        // Process the fetch queue
        processFetchQueue();

        // Wait for a new request or a timeout
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queueCondition.wait_for(lock, std::chrono::seconds(1), [this] {
            return !m_running || !m_fetchQueue.empty();
        });
    }

    getLogger()->debug("Fetch queue thread stopped");
}

void MetadataFetcher::timeoutThread() {
    getLogger()->debug("Timeout thread started");

    while (m_running) {
        // Check for timeouts
        checkTimeouts();

        // Sleep for a while
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    getLogger()->debug("Timeout thread stopped");
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
            getLogger()->debug("Connection established for info hash: {}", infoHashStr);
            break;

        case BTConnectionState::Disconnected:
        case BTConnectionState::Error:
            getLogger()->debug("Connection ended for info hash: {}: {}", infoHashStr, message);

            // If we have a connection, add its endpoint to the failed endpoints list
            if (connection) {
                request->failedEndpoints.push_back(connection->getRemoteEndpoint());
            }

            // Decrement the active connection count
            request->activeConnections--;

            // Check if all connections have ended
            if (request->activeConnections == 0) {
                // Check if we should retry
                if (request->retryCount < m_config.maxRetries) {
                    // Increment retry count
                    request->retryCount++;
                    request->lastRetryTime = std::chrono::steady_clock::now();

                    getLogger()->debug("Retrying metadata fetch for info hash: {} (retry {}/{})",
                                 infoHashStr, request->retryCount, m_config.maxRetries);

                    // Get endpoints that haven't failed yet
                    std::vector<network::EndPoint> remainingEndpoints;
                    for (const auto& endpoint : request->endpoints) {
                        // Check if this endpoint is in the failed endpoints list
                        bool failed = false;
                        for (const auto& failedEndpoint : request->failedEndpoints) {
                            if (endpoint == failedEndpoint) {
                                failed = true;
                                break;
                            }
                        }

                        if (!failed) {
                            remainingEndpoints.push_back(endpoint);
                        }
                    }

                    // If we have remaining endpoints, retry with them
                    if (!remainingEndpoints.empty()) {
                        // Start connections to the remaining endpoints
                        for (size_t i = 0; i < std::min(remainingEndpoints.size(), static_cast<size_t>(m_config.maxConnectionsPerInfoHash)); ++i) {
                            const auto& endpoint = remainingEndpoints[i];

                            // Get a connection from the pool
                            auto pooledConnection = m_connectionPool->getConnection(endpoint, std::chrono::milliseconds(m_config.connectionTimeout));

                            if (pooledConnection) {
                                // Create an async socket
                                auto asyncSocket = std::make_unique<network::AsyncSocket>(pooledConnection->getSocket(), m_multiplexer);

                                // Create a BT connection
                                auto newConnection = std::make_shared<BTConnection>(
                                    std::move(asyncSocket),
                                    request->infoHash,
                                    [this, infoHash = request->infoHash](BTConnectionState state, const std::string& message) {
                                        this->handleConnectionStateChange(infoHash, nullptr, state, message);
                                    },
                                    [this, infoHash = request->infoHash](const std::vector<uint8_t>& metadata, uint32_t size) {
                                        this->handleMetadata(infoHash, nullptr, metadata, size);
                                    },
                                    m_config.peerIdPrefix,
                                    m_config.clientVersion
                                );

                                // Store the connection
                                request->connections.push_back(newConnection);
                                request->activeConnections++;

                                // Connect
                                if (!newConnection->connect(endpoint)) {
                                    getLogger()->error("Failed to connect to {}: {}", endpoint.toString(), "Connection failed");
                                    request->activeConnections--;
                                    continue;
                                }

                                getLogger()->debug("Started retry connection to {} for info hash: {}", endpoint.toString(), infoHashStr);
                            } else {
                                getLogger()->error("Failed to get connection from pool for {}", endpoint.toString());
                            }
                        }

                        // If we couldn't start any connections, complete with failure
                        if (request->activeConnections == 0) {
                            getLogger()->warning("No connections could be established for retry, completing with failure");
                            completeFetch(infoHash, {}, 0, false);
                        }
                    } else {
                        getLogger()->warning("No remaining endpoints to retry, completing with failure");
                        completeFetch(infoHash, {}, 0, false);
                    }
                } else {
                    // We've reached the maximum number of retries, complete with failure
                    getLogger()->warning("Maximum retries reached for info hash: {}, completing with failure", infoHashStr);
                    completeFetch(infoHash, {}, 0, false);
                }
            }
            break;

        default:
            // Other states are not interesting for us
            break;
    }
}

void MetadataFetcher::handleMetadata(
    const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash,
    std::shared_ptr<BTConnection> /* connection */,
    const std::vector<uint8_t>& metadata,
    uint32_t size) {
    try {
        // Validate the metadata before proceeding
        if (metadata.empty() || size == 0 || size > metadata.size()) {
            getLogger()->warning("Received invalid metadata for info hash: {}, size: {}, vector size: {}",
                          infoHashToString(infoHash), size, metadata.size());
            completeFetch(infoHash, {}, 0, false);
            return;
        }

        // Make a deep copy of the metadata to ensure we're not accessing freed memory
        std::vector<uint8_t> metadataCopy = metadata;

        // Complete the fetch with success using the copied metadata
        completeFetch(infoHash, metadataCopy, size, true);
    } catch (const std::exception& e) {
        getLogger()->error("Exception in handleMetadata: {}", e.what());
        completeFetch(infoHash, {}, 0, false);
    } catch (...) {
        getLogger()->error("Unknown exception in handleMetadata");
        completeFetch(infoHash, {}, 0, false);
    }
}

void MetadataFetcher::completeFetch(
    const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash,
    const std::vector<uint8_t>& metadata,
    uint32_t size,
    bool success) {
    try {
        // Make a copy of the info hash to ensure it remains valid
        std::array<uint8_t, BT_INFOHASH_LENGTH> infoHashCopy = infoHash;
        std::string infoHashStr = infoHashToString(infoHashCopy);

        // Make a copy of the metadata if it's valid
        std::vector<uint8_t> metadataCopy;
        if (success && !metadata.empty() && size > 0 && size <= metadata.size()) {
            metadataCopy = metadata;
        }

        // Find the fetch request
        std::shared_ptr<FetchRequest> request;

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            auto it = m_activeFetches.find(infoHashStr);
            if (it == m_activeFetches.end()) {
                getLogger()->warning("Metadata fetch completion for unknown info hash: {}", infoHashStr);
                return;
            }

            request = it->second;

            // Check if the fetch has already completed
            if (request->completed) {
                getLogger()->debug("Metadata fetch already completed for info hash: {}", infoHashStr);
                return;
            }

            // Mark as completed
            request->completed = true;

            // Remove from active fetches
            m_activeFetches.erase(it);
        }

        getLogger()->debug("Metadata fetch completed for info hash: {}, success: {}", infoHashStr, success);

        // Disconnect all connections
        for (auto& connection : request->connections) {
            if (connection) {
                try {
                    connection->disconnect();
                } catch (const std::exception& e) {
                    getLogger()->error("Exception while disconnecting connection: {}", e.what());
                } catch (...) {
                    getLogger()->error("Unknown exception while disconnecting connection");
                }
            }
        }

        // Store the metadata if successful and enabled
        if (success && !metadataCopy.empty()) {
            // Store in metadata storage if enabled
            if (m_config.useMetadataStorage && m_metadataStorage) {
                try {
                    getLogger()->debug("Attempting to store metadata for info hash: {}, size: {}, vector size: {}",
                                 infoHashStr, size, metadataCopy.size());

                    if (m_metadataStorage->addMetadata(infoHashCopy, metadataCopy.data(), size)) {
                        getLogger()->info("Stored metadata for info hash: {}", infoHashStr);
                    } else {
                        getLogger()->error("Failed to store metadata for info hash: {}", infoHashStr);
                    }
                } catch (const std::exception& e) {
                    getLogger()->error("Exception while storing metadata: {}", e.what());
                } catch (...) {
                    getLogger()->error("Unknown exception while storing metadata");
                }
            }
            // Create a torrent file if enabled (this is now handled by the metadata storage)
            else if (m_config.createTorrentFiles) {
                try {
                    // Create a torrent file builder
                    TorrentFileBuilder builder(metadataCopy, size);

                    // Set torrent file properties
                    builder.setAnnounceUrl(m_config.announceUrl);
                    builder.setAnnounceList(m_config.announceList);
                    builder.setCreatedBy(m_config.createdBy);
                    builder.setComment(m_config.comment);

                    // Save the torrent file
                    std::filesystem::path directory(m_config.torrentFilesDirectory);
                    std::string torrentFilePath = builder.saveTorrentFile(directory);

                    if (!torrentFilePath.empty()) {
                        getLogger()->info("Created torrent file: {}", torrentFilePath);
                    } else {
                        getLogger()->error("Failed to create torrent file for info hash: {}", infoHashStr);
                    }
                } catch (const std::exception& e) {
                    getLogger()->error("Exception while creating torrent file: {}", e.what());
                } catch (...) {
                    getLogger()->error("Unknown exception while creating torrent file");
                }
            }
        }

        // Call the callback with a copy of the data to ensure it remains valid
        if (request->callback) {
            try {
                request->callback(infoHashCopy, metadataCopy, size, success);
            } catch (const std::exception& e) {
                getLogger()->error("Exception in metadata fetch callback: {}", e.what());
            } catch (...) {
                getLogger()->error("Unknown exception in metadata fetch callback");
            }
        }

        // Process the fetch queue
        try {
            processFetchQueue();
        } catch (const std::exception& e) {
            getLogger()->error("Exception in processFetchQueue: {}", e.what());
        } catch (...) {
            getLogger()->error("Unknown exception in processFetchQueue");
        }
    } catch (const std::exception& e) {
        getLogger()->error("Exception in completeFetch: {}", e.what());
    } catch (...) {
        getLogger()->error("Unknown exception in completeFetch");
    }
}

} // namespace dht_hunter::metadata
