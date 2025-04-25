#pragma once

#include "dht_hunter/dht/core/dht_node.hpp"
#include "dht_hunter/crawler/infohash_collector.hpp"
#include "dht_hunter/metadata/metadata_fetcher.hpp"
#include "dht_hunter/storage/metadata_storage.hpp"
#include "dht_hunter/logforge/logforge.hpp"

#include <memory>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <functional>

namespace dht_hunter::crawler {

/**
 * @brief Configuration for the DHT crawler
 */
struct DHTCrawlerConfig {
    // DHT node configuration
    uint16_t dhtPort = 6889;                                ///< Port for the DHT node (using a higher port to avoid potential blocks)
    std::string configDir = "config";                       ///< Configuration directory
    size_t kBucketSize = 16;                                ///< Maximum number of nodes in a k-bucket
    size_t lookupAlpha = 5;                                ///< Alpha parameter for parallel lookups
    size_t lookupMaxResults = 16;                          ///< Maximum number of nodes to store in a lookup result
    bool saveRoutingTableOnNewNode = true;                 ///< Whether to save the routing table after each new node is added
    std::vector<std::string> bootstrapNodes = {             ///< Bootstrap nodes for the DHT node
        // Standard bootstrap nodes
        "router.bittorrent.com:6881",
        "dht.transmissionbt.com:6881",
        "router.utorrent.com:6881",
        "router.bitcomet.com:6881",
        "dht.aelitis.com:6881",

        // Nodes with non-standard ports
        "dht.libtorrent.org:25401",
        "dht.anacrolix.link:6881",
        "router.silotis.us:6881",
        "dht.bluishcoder.co.nz:6881",
        "dht.mldht.org:6881",
        "dht.vuze.com:6881",

        // Additional nodes with different ports
        "router.bittorrent.com:6882",
        "router.bittorrent.com:6883",
        "router.bittorrent.com:6884",
        "router.bittorrent.com:6885",
        "dht.transmissionbt.com:6882",
        "dht.transmissionbt.com:6883",
        "router.utorrent.com:6882",
        "router.utorrent.com:6883",

        // Public DHT bootstrap nodes
        "node.bitdewy.com:6881",
        "dht.aelitis.com:6881",
        "dht.filecxx.com:6881",
        "dht.filecxx.com:6882",
        "dht.filecxx.com:6883",
        "dht.filecxx.com:6884",
        "dht.filecxx.com:6885",
        "dht.filecxx.com:6886",
        "dht.filecxx.com:6887",
        "dht.filecxx.com:6888",
        "dht.filecxx.com:6889"
    };

    // Client identification
    std::string peerIdPrefix = "-DH0001-";                  ///< Peer ID prefix for BitTorrent handshakes
    std::string clientVersion = "DHT-Hunter 0.1.0";         ///< Client version string for extension protocol

    // Crawling configuration
    uint32_t maxConcurrentLookups = 10;                     ///< Maximum number of concurrent lookups
    uint32_t maxConcurrentMetadataFetches = 10;             ///< Maximum number of concurrent metadata fetches
    std::chrono::milliseconds lookupInterval{100};          ///< Interval between lookups
    std::chrono::milliseconds metadataFetchInterval{100};   ///< Interval between metadata fetches
    std::chrono::seconds statusInterval{10};                ///< Interval for status updates

    // Rate limiting
    uint32_t maxLookupsPerMinute = 60;                      ///< Maximum number of lookups per minute
    uint32_t maxMetadataFetchesPerMinute = 60;              ///< Maximum number of metadata fetches per minute

    // Prioritization
    bool prioritizePopularInfoHashes = true;                ///< Whether to prioritize popular info hashes

    // Storage configuration
    bool useInfoHashCollector = true;                       ///< Whether to use the info hash collector
    bool useMetadataStorage = true;                         ///< Whether to use metadata storage

    // Active node discovery
    bool enableActiveNodeDiscovery = true;                  ///< Whether to enable active node discovery
    uint32_t activeNodeDiscoveryInterval = 60;             ///< Interval in seconds for active node discovery

    // Metadata fetcher configuration
    metadata::MetadataFetcherConfig metadataFetcherConfig;  ///< Configuration for the metadata fetcher

    // Info hash collector configuration
    InfoHashCollectorConfig infoHashCollectorConfig;        ///< Configuration for the info hash collector

    // Metadata storage configuration
    std::string metadataStorageDirectory = "config/metadata";  ///< Directory for storing metadata
};

/**
 * @brief Callback for crawler status updates
 * @param infoHashesDiscovered The number of info hashes discovered in this session
 * @param infoHashesQueued The number of info hashes queued for metadata fetching
 * @param metadataFetched The number of metadata items fetched
 * @param lookupRate The lookup rate (lookups per minute)
 * @param metadataFetchRate The metadata fetch rate (fetches per minute)
 * @param totalInfoHashes The total number of info hashes, including those loaded from disk
 * @param routingTableSize The number of nodes in the routing table
 * @param memoryUsage The memory usage in bytes
 */
using CrawlerStatusCallback = std::function<void(
    uint64_t infoHashesDiscovered,
    uint64_t infoHashesQueued,
    uint64_t metadataFetched,
    double lookupRate,
    double metadataFetchRate,
    uint64_t totalInfoHashes,
    size_t routingTableSize,
    uint64_t memoryUsage)>;

/**
 * @brief Class for crawling the DHT network to discover info hashes and fetch metadata
 */
class DHTCrawler {
public:
    /**
     * @brief Constructs a DHT crawler
     * @param config The configuration
     */
    explicit DHTCrawler(const DHTCrawlerConfig& config = DHTCrawlerConfig());

    /**
     * @brief Destructor
     */
    ~DHTCrawler();

    /**
     * @brief Starts the crawler
     * @return True if started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stops the crawler
     */
    void stop();

    /**
     * @brief Checks if the crawler is running
     * @return True if running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Gets the DHT node
     * @return The DHT node
     */
    std::shared_ptr<dht::DHTNode> getDHTNode() const;

    /**
     * @brief Gets the info hash collector
     * @return The info hash collector
     */
    std::shared_ptr<InfoHashCollector> getInfoHashCollector() const;

    /**
     * @brief Gets the metadata fetcher
     * @return The metadata fetcher
     */
    std::shared_ptr<metadata::MetadataFetcher> getMetadataFetcher() const;

    /**
     * @brief Gets the metadata storage
     * @return The metadata storage
     */
    std::shared_ptr<storage::MetadataStorage> getMetadataStorage() const;

    /**
     * @brief Checks if metadata storage is available
     * @return True if available, false otherwise
     */
    bool isMetadataStorageAvailable() const;

    /**
     * @brief Gets the configuration
     * @return The configuration
     */
    DHTCrawlerConfig getConfig() const;

    /**
     * @brief Sets the configuration
     * @param config The configuration
     */
    void setConfig(const DHTCrawlerConfig& config);

    /**
     * @brief Sets the status callback
     * @param callback The callback function
     */
    void setStatusCallback(CrawlerStatusCallback callback);

    /**
     * @brief Gets the number of info hashes discovered
     * @return The number of info hashes discovered
     */
    uint64_t getInfoHashesDiscovered() const;

    /**
     * @brief Gets the number of info hashes queued for metadata fetching
     * @return The number of info hashes queued for metadata fetching
     */
    uint64_t getInfoHashesQueued() const;

    /**
     * @brief Gets the number of metadata items fetched
     * @return The number of metadata items fetched
     */
    uint64_t getMetadataFetched() const;

    /**
     * @brief Gets the lookup rate (lookups per minute)
     * @return The lookup rate
     */
    double getLookupRate() const;

    /**
     * @brief Gets the metadata fetch rate (fetches per minute)
     * @return The metadata fetch rate
     */
    double getMetadataFetchRate() const;

    /**
     * @brief Gets the total number of info hashes, including those loaded from disk
     * @return The total number of info hashes
     */
    uint64_t getTotalInfoHashes() const;

private:
    /**
     * @brief Structure for a lookup request
     */
    struct LookupRequest {
        dht::NodeID targetID;                                ///< Target ID for the lookup
        std::chrono::steady_clock::time_point requestTime;   ///< Time when the lookup was requested
        std::chrono::steady_clock::time_point startTime;     ///< Time when the lookup was started
        std::chrono::steady_clock::time_point endTime;       ///< Time when the lookup was completed
        bool completed = false;                              ///< Whether the lookup has completed
    };

    /**
     * @brief Structure for a metadata fetch request
     */
    struct MetadataFetchRequest {
        dht::InfoHash infoHash;                              ///< Info hash to fetch metadata for
        std::chrono::steady_clock::time_point requestTime;   ///< Time when the fetch was requested
        std::chrono::steady_clock::time_point startTime;     ///< Time when the fetch was started
        std::chrono::steady_clock::time_point endTime;       ///< Time when the fetch was completed
        bool completed = false;                              ///< Whether the fetch has completed
        uint32_t priority = 0;                               ///< Priority of the fetch (higher = more important)

        // Comparison operator for priority queue (higher priority comes first)
        bool operator<(const MetadataFetchRequest& other) const {
            return priority < other.priority;
        }
    };

    /**
     * @brief Initializes the DHT node
     * @return True if initialized successfully, false otherwise
     */
    bool initializeDHTNode();

    /**
     * @brief Initializes the info hash collector
     * @return True if initialized successfully, false otherwise
     */
    bool initializeInfoHashCollector();

    /**
     * @brief Initializes the metadata fetcher
     * @return True if initialized successfully, false otherwise
     */
    bool initializeMetadataFetcher();

    /**
     * @brief Initializes the metadata storage
     * @return True if initialized successfully, false otherwise
     */
    bool initializeMetadataStorage();

    /**
     * @brief Performs a lookup for a random node ID
     */
    void performRandomLookup();

    /**
     * @brief Handles a lookup completion
     * @param targetID The target ID of the lookup
     * @param nodes The nodes found in the lookup
     */
    void handleLookupCompletion(const dht::NodeID& targetID, const std::vector<std::shared_ptr<dht::Node>>& nodes);

    /**
     * @brief Fetches metadata for an info hash
     * @param infoHash The info hash to fetch metadata for
     */
    void fetchMetadata(const dht::InfoHash& infoHash);

    /**
     * @brief Handles metadata fetch completion
     * @param infoHash The info hash
     * @param metadata The metadata
     * @param size The size of the metadata
     * @param success Whether the fetch was successful
     */
    void handleMetadataFetchCompletion(
        const dht::InfoHash& infoHash,
        const std::vector<uint8_t>& metadata,
        uint32_t size,
        bool success);

    /**
     * @brief Handles a new info hash from the collector
     * @param infoHash The info hash
     */
    void handleNewInfoHash(const dht::InfoHash& infoHash);

    /**
     * @brief Lookup thread function
     */
    void lookupThread();

    /**
     * @brief Metadata fetch thread function
     */
    void metadataFetchThread();

    /**
     * @brief Status thread function
     */
    void statusThread();

    /**
     * @brief Updates the terminal window title with current stats
     */
    void updateWindowTitle();

    /**
     * @brief Active node discovery thread function
     */
    void activeNodeDiscoveryThread();

    /**
     * @brief Updates the lookup rate
     */
    void updateLookupRate();

    /**
     * @brief Updates the metadata fetch rate
     */
    void updateMetadataFetchRate();

    /**
     * @brief Checks if a lookup is allowed by rate limiting
     * @return True if allowed, false otherwise
     */
    bool isLookupAllowed();

    /**
     * @brief Checks if a metadata fetch is allowed by rate limiting
     * @return True if allowed, false otherwise
     */
    bool isMetadataFetchAllowed();

    /**
     * @brief Prioritizes info hashes for metadata fetching
     */
    void prioritizeInfoHashes();

    /**
     * @brief Converts an info hash to a hex string
     * @param infoHash The info hash
     * @return The hex string
     */
    std::string infoHashToString(const dht::InfoHash& infoHash) const;

    DHTCrawlerConfig m_config;                                  ///< Configuration
    std::atomic<bool> m_running{false};                         ///< Whether the crawler is running
    mutable std::mutex m_mutex;                                 ///< Mutex for thread safety

    std::shared_ptr<dht::DHTNode> m_dhtNode;                    ///< DHT node
    std::shared_ptr<InfoHashCollector> m_infoHashCollector;     ///< Info hash collector
    std::shared_ptr<metadata::MetadataFetcher> m_metadataFetcher; ///< Metadata fetcher
    std::shared_ptr<storage::MetadataStorage> m_metadataStorage; ///< Metadata storage
    std::atomic<bool> m_metadataStorageAvailable{false};        ///< Whether metadata storage is available

    std::thread m_lookupThread;                                 ///< Thread for performing lookups
    std::thread m_metadataFetchThread;                          ///< Thread for fetching metadata
    std::thread m_statusThread;                                 ///< Thread for status updates
    std::thread m_activeNodeDiscoveryThread;                    ///< Thread for active node discovery

    std::queue<LookupRequest> m_lookupQueue;                    ///< Queue of lookup requests
    std::priority_queue<MetadataFetchRequest> m_metadataFetchQueue; ///< Queue of metadata fetch requests

    std::unordered_map<std::string, LookupRequest> m_activeLookups; ///< Active lookups by target ID (hex string)
    std::unordered_map<std::string, MetadataFetchRequest> m_activeMetadataFetches; ///< Active metadata fetches by info hash (hex string)

    std::unordered_set<std::string> m_processedInfoHashes;      ///< Set of processed info hashes (hex string)

    std::atomic<uint64_t> m_infoHashesDiscovered{0};            ///< Number of info hashes discovered
    std::atomic<uint64_t> m_infoHashesQueued{0};                ///< Number of info hashes queued for metadata fetching
    std::atomic<uint64_t> m_metadataFetched{0};                 ///< Number of metadata items fetched

    std::atomic<double> m_lookupRate{0.0};                      ///< Lookup rate (lookups per minute)
    std::atomic<double> m_metadataFetchRate{0.0};               ///< Metadata fetch rate (fetches per minute)

    std::vector<std::chrono::steady_clock::time_point> m_lookupTimes; ///< Times of recent lookups
    std::vector<std::chrono::steady_clock::time_point> m_metadataFetchTimes; ///< Times of recent metadata fetches

    CrawlerStatusCallback m_statusCallback;                     ///< Callback for status updates
};

} // namespace dht_hunter::crawler
