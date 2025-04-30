#pragma once

#include "dht_hunter/dht/crawler/components/base_crawler_component.hpp"
#include "dht_hunter/dht/crawler/components/crawler_config.hpp"
#include "dht_hunter/dht/crawler/components/crawler_statistics.hpp"
#include "dht_hunter/dht/routing/routing_manager.hpp"
#include "dht_hunter/dht/node_lookup/node_lookup.hpp"
#include "dht_hunter/dht/peer_lookup/peer_lookup.hpp"
#include "dht_hunter/dht/transactions/transaction_manager.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include "dht_hunter/dht/storage/peer_storage.hpp"
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <random>

namespace dht_hunter::dht::crawler {

/**
 * @brief Manager for the crawler
 * 
 * This component manages the crawler, including discovering nodes, monitoring info hashes,
 * and collecting statistics.
 */
class CrawlerManager : public BaseCrawlerComponent {
public:
    /**
     * @brief Gets the singleton instance
     * @param config The DHT configuration
     * @param nodeID The node ID
     * @param routingManager The routing manager
     * @param nodeLookup The node lookup
     * @param peerLookup The peer lookup
     * @param transactionManager The transaction manager
     * @param messageSender The message sender
     * @param peerStorage The peer storage
     * @param crawlerConfig The crawler configuration
     * @return The singleton instance
     */
    static std::shared_ptr<CrawlerManager> getInstance(
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
    ~CrawlerManager() override;

    /**
     * @brief Gets the crawler statistics
     * @return The crawler statistics
     */
    CrawlerStatistics getStatistics() const;

    /**
     * @brief Gets the discovered nodes
     * @param maxNodes The maximum number of nodes to return (0 for all)
     * @return The discovered nodes
     */
    std::vector<std::shared_ptr<Node>> getDiscoveredNodes(size_t maxNodes = 0) const;

    /**
     * @brief Gets the discovered info hashes
     * @param maxInfoHashes The maximum number of info hashes to return (0 for all)
     * @return The discovered info hashes
     */
    std::vector<InfoHash> getDiscoveredInfoHashes(size_t maxInfoHashes = 0) const;

    /**
     * @brief Gets the peers for an info hash
     * @param infoHash The info hash
     * @return The peers for the info hash
     */
    std::vector<EndPoint> getPeersForInfoHash(const InfoHash& infoHash) const;

    /**
     * @brief Monitors an info hash
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

protected:
    /**
     * @brief Initializes the component
     * @return True if the component was initialized successfully, false otherwise
     */
    bool onInitialize() override;

    /**
     * @brief Starts the component
     * @return True if the component was started successfully, false otherwise
     */
    bool onStart() override;

    /**
     * @brief Stops the component
     */
    void onStop() override;

    /**
     * @brief Performs a crawl iteration
     */
    void onCrawl() override;

private:
    /**
     * @brief Private constructor for singleton pattern
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
    CrawlerManager(const DHTConfig& config,
                  const NodeID& nodeID,
                  std::shared_ptr<RoutingManager> routingManager,
                  std::shared_ptr<NodeLookup> nodeLookup,
                  std::shared_ptr<PeerLookup> peerLookup,
                  std::shared_ptr<TransactionManager> transactionManager,
                  std::shared_ptr<MessageSender> messageSender,
                  std::shared_ptr<PeerStorage> peerStorage,
                  const CrawlerConfig& crawlerConfig);

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

    /**
     * @brief Crawl thread function
     */
    void crawlThread();

    // Static members
    static std::shared_ptr<CrawlerManager> s_instance;
    static std::mutex s_instanceMutex;

    // Configuration
    CrawlerConfig m_crawlerConfig;

    // Components
    std::shared_ptr<RoutingManager> m_routingManager;
    std::shared_ptr<NodeLookup> m_nodeLookup;
    std::shared_ptr<PeerLookup> m_peerLookup;
    std::shared_ptr<TransactionManager> m_transactionManager;
    std::shared_ptr<MessageSender> m_messageSender;
    std::shared_ptr<PeerStorage> m_peerStorage;

    // Crawler state
    std::thread m_crawlThread;

    // Crawler data
    std::unordered_map<std::string, std::shared_ptr<Node>> m_discoveredNodes;
    std::unordered_map<std::string, InfoHash> m_discoveredInfoHashes;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_infoHashPeers;
    std::unordered_set<std::string> m_monitoredInfoHashes;

    // Statistics
    CrawlerStatistics m_statistics;

    // Event subscription IDs
    std::vector<int> m_eventSubscriptionIds;

    // Random number generator
    std::mt19937 m_rng;
};

} // namespace dht_hunter::dht::crawler
