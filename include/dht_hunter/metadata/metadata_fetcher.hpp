#pragma once

#include "dht_hunter/metadata/bt_connection.hpp"
#include "dht_hunter/metadata/torrent_file_builder.hpp"
#include "dht_hunter/storage/metadata_storage.hpp"
#include "dht_hunter/network/connection_pool_manager.hpp"
#include "dht_hunter/network/async_socket_factory.hpp"
#include "dht_hunter/network/io_multiplexer.hpp"

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <queue>
#include <atomic>
#include <thread>
#include <condition_variable>

namespace dht_hunter::metadata {

/**
 * @brief Callback for metadata fetch completion
 * @param infoHash The info hash of the torrent
 * @param metadata The metadata
 * @param size The size of the metadata
 * @param success Whether the fetch was successful
 */
using MetadataFetchCallback = std::function<void(
    const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash,
    const std::vector<uint8_t>& metadata,
    uint32_t size,
    bool success)>;

/**
 * @brief Configuration for the metadata fetcher
 */
struct MetadataFetcherConfig {
    uint32_t maxConcurrentFetches = 10;                 ///< Maximum number of concurrent fetches
    uint32_t maxConnectionsPerInfoHash = 3;             ///< Maximum number of connections per info hash
    std::chrono::seconds connectionTimeout{30};         ///< Connection timeout
    std::chrono::seconds fetchTimeout{120};             ///< Fetch timeout
    std::string connectionPoolName = "metadata";        ///< Name of the connection pool to use
    bool reuseConnections = true;                       ///< Whether to reuse connections
    uint32_t maxRetries = 3;                            ///< Maximum number of retries for failed connections
    std::chrono::seconds retryDelay{1};                 ///< Delay between retries

    // Client identification
    std::string peerIdPrefix = "-DH0001-";              ///< Peer ID prefix for BitTorrent handshakes
    std::string clientVersion = "DHT-Hunter 0.1.0";     ///< Client version string for extension protocol

    // Torrent file construction options
    bool createTorrentFiles = true;                     ///< Whether to create torrent files
    std::string torrentFilesDirectory = "config/torrents";     ///< Directory to save torrent files
    std::string announceUrl = "";                       ///< Announce URL for created torrent files
    std::vector<std::vector<std::string>> announceList; ///< Announce list for created torrent files
    std::string createdBy = "DHT-Hunter";               ///< Created by string for created torrent files
    std::string comment = "";                           ///< Comment for created torrent files

    // Metadata storage options
    bool useMetadataStorage = true;                     ///< Whether to use metadata storage
    std::string metadataStorageDirectory = "config/metadata";  ///< Directory to store metadata
    bool persistMetadata = true;                        ///< Whether to persist metadata to disk
};

/**
 * @class MetadataFetcher
 * @brief Fetches metadata for torrents using the BitTorrent protocol
 *
 * This class manages multiple concurrent metadata fetches, using a connection
 * pool to efficiently reuse connections.
 */
class MetadataFetcher {
public:
    /**
     * @brief Constructs a metadata fetcher
     * @param config The configuration
     */
    explicit MetadataFetcher(const MetadataFetcherConfig& config = MetadataFetcherConfig());

    /**
     * @brief Destructor
     */
    ~MetadataFetcher();

    /**
     * @brief Starts the metadata fetcher
     * @return True if started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stops the metadata fetcher
     */
    void stop();

    /**
     * @brief Checks if the metadata fetcher is running
     * @return True if running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Fetches metadata for a torrent
     * @param infoHash The info hash of the torrent
     * @param endpoints The endpoints to connect to
     * @param callback The callback to call when the fetch is complete
     * @return True if the fetch was started successfully, false otherwise
     */
    bool fetchMetadata(
        const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash,
        const std::vector<network::EndPoint>& endpoints,
        MetadataFetchCallback callback);

    /**
     * @brief Cancels a metadata fetch
     * @param infoHash The info hash of the torrent
     * @return True if the fetch was canceled, false if it wasn't found
     */
    bool cancelFetch(const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash);

    /**
     * @brief Gets the number of active fetches
     * @return The number of active fetches
     */
    size_t getActiveFetchCount() const;

    /**
     * @brief Gets the number of queued fetches
     * @return The number of queued fetches
     */
    size_t getQueuedFetchCount() const;

    /**
     * @brief Sets the configuration
     * @param config The configuration
     */
    void setConfig(const MetadataFetcherConfig& config);

    /**
     * @brief Gets the configuration
     * @return The configuration
     */
    MetadataFetcherConfig getConfig() const;

    /**
     * @brief Gets the metadata storage
     * @return The metadata storage
     */
    std::shared_ptr<storage::MetadataStorage> getMetadataStorage() const;

    /**
     * @brief Sets the metadata storage
     * @param metadataStorage The metadata storage to use
     */
    void setMetadataStorage(std::shared_ptr<storage::MetadataStorage> metadataStorage);

private:
    /**
     * @brief Structure for a metadata fetch request
     */
    struct FetchRequest {
        std::array<uint8_t, BT_INFOHASH_LENGTH> infoHash;  ///< Info hash of the torrent
        std::vector<network::EndPoint> endpoints;          ///< Endpoints to connect to
        MetadataFetchCallback callback;                    ///< Callback to call when the fetch is complete
        std::chrono::steady_clock::time_point startTime;   ///< Time when the fetch was started
        std::vector<std::shared_ptr<BTConnection>> connections; ///< Active connections
        std::atomic<uint32_t> activeConnections{0};        ///< Number of active connections
        std::atomic<bool> completed{false};                ///< Whether the fetch has completed
        std::atomic<uint32_t> retryCount{0};               ///< Number of retries attempted
        std::chrono::steady_clock::time_point lastRetryTime; ///< Time of the last retry
        std::vector<network::EndPoint> failedEndpoints;    ///< Endpoints that failed to connect
    };

    /**
     * @brief Processes the fetch queue
     */
    void processFetchQueue();

    /**
     * @brief Checks for timed out fetches
     */
    void checkTimeouts();

    /**
     * @brief Thread function for processing the fetch queue
     */
    void fetchQueueThread();

    /**
     * @brief Thread function for checking timeouts
     */
    void timeoutThread();

    /**
     * @brief Handles a connection state change
     * @param infoHash The info hash of the torrent
     * @param connection The connection
     * @param state The new state
     * @param message The state message
     */
    void handleConnectionStateChange(
        const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash,
        std::shared_ptr<BTConnection> connection,
        BTConnectionState state,
        const std::string& message);

    /**
     * @brief Handles metadata reception
     * @param infoHash The info hash of the torrent
     * @param connection The connection
     * @param metadata The metadata
     * @param size The size of the metadata
     */
    void handleMetadata(
        const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash,
        std::shared_ptr<BTConnection> connection,
        const std::vector<uint8_t>& metadata,
        uint32_t size);

    /**
     * @brief Completes a fetch
     * @param infoHash The info hash of the torrent
     * @param metadata The metadata
     * @param size The size of the metadata
     * @param success Whether the fetch was successful
     */
    void completeFetch(
        const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash,
        const std::vector<uint8_t>& metadata,
        uint32_t size,
        bool success);

    MetadataFetcherConfig m_config;                                 ///< Configuration
    std::shared_ptr<network::IOMultiplexer> m_multiplexer;          ///< I/O multiplexer
    std::shared_ptr<network::ConnectionPool> m_connectionPool;      ///< Connection pool
    std::shared_ptr<storage::MetadataStorage> m_metadataStorage;    ///< Metadata storage

    std::unordered_map<std::string, std::shared_ptr<FetchRequest>> m_activeFetches; ///< Active fetches by info hash (hex string)
    std::queue<std::shared_ptr<FetchRequest>> m_fetchQueue;         ///< Queue of fetch requests

    mutable std::mutex m_mutex;                                     ///< Mutex for thread safety
    std::condition_variable m_queueCondition;                       ///< Condition variable for the fetch queue

    std::atomic<bool> m_running{false};                             ///< Whether the fetcher is running
    std::thread m_fetchQueueThread;                                 ///< Thread for processing the fetch queue
    std::thread m_timeoutThread;                                    ///< Thread for checking timeouts
};

} // namespace dht_hunter::metadata
