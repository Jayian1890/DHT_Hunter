#pragma once

#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/dht/types/dht_types.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"
#include "dht_hunter/unified_event/event_bus.hpp"
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <chrono>

namespace dht_hunter::dht {

// Forward declarations
class RoutingManager;
class NodeLookup;
class PeerLookup;
class TransactionManager;
class MessageSender;
class PeerStorage;

/**
 * @brief Configuration for the crawler
 */
struct CrawlerConfig {
    // How many nodes to crawl in parallel
    size_t parallelCrawls = 10;
    
    // How often to refresh the crawler (in seconds)
    uint32_t refreshInterval = 60;
    
    // Maximum number of nodes to store
    size_t maxNodes = 10000;
    
    // Maximum number of info hashes to track
    size_t maxInfoHashes = 1000;
    
    // Whether to automatically start crawling on initialization
    bool autoStart = true;
};

/**
 * @brief Statistics collected by the crawler
 */
struct CrawlerStatistics {
    // Number of nodes discovered
    size_t nodesDiscovered = 0;
    
    // Number of nodes that responded
    size_t nodesResponded = 0;
    
    // Number of info hashes discovered
    size_t infoHashesDiscovered = 0;
    
    // Number of peers discovered
    size_t peersDiscovered = 0;
    
    // Number of queries sent
    size_t queriesSent = 0;
    
    // Number of responses received
    size_t responsesReceived = 0;
    
    // Number of errors received
    size_t errorsReceived = 0;
    
    // Number of timeouts
    size_t timeouts = 0;
    
    // Time when the crawler started
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
};

/**
 * @brief DHT network crawler (Singleton)
 * 
 * The crawler systematically explores the DHT network to discover nodes,
 * monitor info hashes, and collect statistics.
 */
class Crawler {
public:
    /**
     * @brief Gets the singleton instance of the crawler
     * @param config The DHT configuration (only used if instance doesn't exist yet)
     * @param nodeID The node ID (only used if instance doesn't exist yet)
     * @param routingManager The routing manager (only used if instance doesn't exist yet)
     * @param nodeLookup The node lookup (only used if instance doesn't exist yet)
     * @param peerLookup The peer lookup (only used if instance doesn't exist yet)
     * @param transactionManager The transaction manager (only used if instance doesn't exist yet)
     * @param messageSender The message sender (only used if instance doesn't exist yet)
     * @param peerStorage The peer storage (only used if instance doesn't exist yet)
     * @param crawlerConfig The crawler configuration (only used if instance doesn't exist yet)
     * @return The singleton instance
     */
    static std::shared_ptr<Crawler> getInstance(
        const DHTConfig& config,
        const NodeID& nodeID,
        std::shared_ptr<RoutingManager> routingManager,
        std::shared_ptr<NodeLookup> nodeLookup,
        std::shared_ptr<PeerLookup> peerLookup,
        std::shared_ptr<TransactionManager> transactionManager,
        std::shared_ptr<MessageSender> messageSender,
        std::shared_ptr<PeerStorage> peerStorage,
        const CrawlerConfig& crawlerConfig = CrawlerConfig());

    /**
     * @brief Destructor
     */
    ~Crawler();

    /**
     * @brief Delete copy constructor and assignment operator
     */
    Crawler(const Crawler&) = delete;
    Crawler& operator=(const Crawler&) = delete;
    Crawler(Crawler&&) = delete;
    Crawler& operator=(Crawler&&) = delete;

    /**
     * @brief Starts the crawler
     * @return True if the crawler was started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stops the crawler
     */
    void stop();

    /**
     * @brief Checks if the crawler is running
     * @return True if the crawler is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Gets the crawler statistics
     * @return The crawler statistics
     */
    CrawlerStatistics getStatistics() const;

    /**
     * @brief Gets the discovered nodes
     * @param maxNodes Maximum number of nodes to return (0 = all)
     * @return The discovered nodes
     */
    std::vector<std::shared_ptr<Node>> getDiscoveredNodes(size_t maxNodes = 0) const;

    /**
     * @brief Gets the discovered info hashes
     * @param maxInfoHashes Maximum number of info hashes to return (0 = all)
     * @return The discovered info hashes
     */
    std::vector<InfoHash> getDiscoveredInfoHashes(size_t maxInfoHashes = 0) const;

    /**
     * @brief Gets the peers for an info hash
     * @param infoHash The info hash
     * @return The peers for the info hash
     */
    std::vector<network::EndPoint> getPeersForInfoHash(const InfoHash& infoHash) const;

    /**
     * @brief Adds an info hash to monitor
     * @param infoHash The info hash to monitor
     */
    void monitorInfoHash(const InfoHash& infoHash);

    /**
     * @brief Stops monitoring an info hash
     * @param infoHash The info hash to stop monitoring
     */
    void stopMonitoringInfoHash(const InfoHash& infoHash);

    /**
     * @brief Checks if an info hash is being monitored
     * @param infoHash The info hash to check
     * @return True if the info hash is being monitored, false otherwise
     */
    bool isMonitoringInfoHash(const InfoHash& infoHash) const;

    /**
     * @brief Gets the monitored info hashes
     * @return The monitored info hashes
     */
    std::vector<InfoHash> getMonitoredInfoHashes() const;

private:
    /**
     * @brief Constructor
     * @param config The DHT configuration
     * @param nodeID The node ID
     * @param routingManager The routing manager
     * @param nodeLookup The node lookup
     * @param peerLookup The peer lookup
     * @param transactionManager The transaction manager
     * @param messageSender The message sender
     * @param peerStorage The peer storage
     * @param crawlerConfig The crawler configuration
     */
    Crawler(const DHTConfig& config,
            const NodeID& nodeID,
            std::shared_ptr<RoutingManager> routingManager,
            std::shared_ptr<NodeLookup> nodeLookup,
            std::shared_ptr<PeerLookup> peerLookup,
            std::shared_ptr<TransactionManager> transactionManager,
            std::shared_ptr<MessageSender> messageSender,
            std::shared_ptr<PeerStorage> peerStorage,
            const CrawlerConfig& crawlerConfig);

    /**
     * @brief Performs a crawl iteration
     */
    void crawl();

    /**
     * @brief Crawl thread function
     */
    void crawlThread();

    /**
     * @brief Discovers new nodes
     */
    void discoverNodes();

    /**
     * @brief Monitors info hashes
     */
    void monitorInfoHashes();

    /**
     * @brief Updates statistics
     */
    void updateStatistics();

    /**
     * @brief Handles a node discovered event
     * @param event The event
     */
    void handleNodeDiscoveredEvent(const std::shared_ptr<unified_event::Event>& event);

    /**
     * @brief Handles a peer discovered event
     * @param event The event
     */
    void handlePeerDiscoveredEvent(const std::shared_ptr<unified_event::Event>& event);

    /**
     * @brief Handles a message received event
     * @param event The event
     */
    void handleMessageReceivedEvent(const std::shared_ptr<unified_event::Event>& event);

    /**
     * @brief Handles a message sent event
     * @param event The event
     */
    void handleMessageSentEvent(const std::shared_ptr<unified_event::Event>& event);

    // Static members
    static std::shared_ptr<Crawler> s_instance;
    static std::mutex s_instanceMutex;

    // Configuration
    DHTConfig m_config;
    NodeID m_nodeID;
    CrawlerConfig m_crawlerConfig;

    // Components
    std::shared_ptr<RoutingManager> m_routingManager;
    std::shared_ptr<NodeLookup> m_nodeLookup;
    std::shared_ptr<PeerLookup> m_peerLookup;
    std::shared_ptr<TransactionManager> m_transactionManager;
    std::shared_ptr<MessageSender> m_messageSender;
    std::shared_ptr<PeerStorage> m_peerStorage;
    std::shared_ptr<unified_event::EventBus> m_eventBus;

    // Crawler state
    bool m_running;
    std::thread m_crawlThread;
    mutable std::mutex m_mutex;

    // Crawler data
    std::unordered_map<std::string, std::shared_ptr<Node>> m_discoveredNodes;
    std::unordered_map<std::string, InfoHash> m_discoveredInfoHashes;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_infoHashPeers;
    std::unordered_set<std::string> m_monitoredInfoHashes;

    // Statistics
    CrawlerStatistics m_statistics;

    // Event subscription IDs
    std::vector<int> m_eventSubscriptionIds;
};

} // namespace dht_hunter::dht
