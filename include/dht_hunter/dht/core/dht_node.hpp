#pragma once

#include "dht_hunter/dht/types.hpp"
#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/dht/core/routing_table.hpp"
#include "dht_hunter/dht/extensions/dht_extension.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include "dht_hunter/dht/services/statistics_service.hpp"
#include "dht_hunter/bittorrent/bt_message_handler.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

namespace dht_hunter::dht {

// Forward declarations
class SocketManager;
class MessageSender;
class MessageHandler;
class BTMessageHandler;
class TokenManager;
class PeerStorage;
class TransactionManager;
class RoutingManager;
class NodeLookup;
class PeerLookup;
class Bootstrapper;
class Crawler;

namespace extensions {
    class DHTExtension;
    class MainlineDHT;
    class KademliaDHT;
    class AzureusDHT;
}

/**
 * @brief A DHT node
 */
class DHTNode {
public:
    /**
     * @brief Constructs a DHT node
     * @param config The DHT configuration
     */
    explicit DHTNode(const DHTConfig& config = DHTConfig());

    /**
     * @brief Destructor
     */
    ~DHTNode();

    /**
     * @brief Starts the DHT node
     * @return True if the node was started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stops the DHT node
     */
    void stop();

    /**
     * @brief Checks if the DHT node is running
     * @return True if the node is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Gets the node ID
     * @return The node ID
     */
    const NodeID& getNodeID() const;

    /**
     * @brief Gets the port
     * @return The port
     */
    uint16_t getPort() const;

    /**
     * @brief Gets the statistics as a JSON string
     * @return The statistics as a JSON string
     */
    std::string getStatistics() const;

    /**
     * @brief Handles a BitTorrent PORT message
     * @param peerAddress The peer's address
     * @param data The message data
     * @param length The message length
     * @return True if the message was handled successfully, false otherwise
     */
    bool handlePortMessage(const network::NetworkAddress& peerAddress, const uint8_t* data, size_t length) const;

    /**
     * @brief Handles a BitTorrent PORT message
     * @param peerAddress The peer's address
     * @param port The DHT port
     * @return True if the message was handled successfully, false otherwise
     */
    bool handlePortMessage(const network::NetworkAddress& peerAddress, uint16_t port) const;

    /**
     * @brief Creates a BitTorrent PORT message
     * @return The PORT message
     */
    std::vector<uint8_t> createPortMessage() const;

    /**
     * @brief Gets the routing table
     * @return The routing table
     */
    std::shared_ptr<RoutingTable> getRoutingTable() const;

    /**
     * @brief Finds the closest nodes to a target ID
     * @param targetID The target ID
     * @param callback The callback to call with the result
     */
    void findClosestNodes(const NodeID& targetID,
        const std::function<void(const std::vector<std::shared_ptr<Node>> &)>
            &callback) const;

    /**
     * @brief Gets peers for an info hash
     * @param infoHash The info hash
     * @param callback The callback to call with the result
     */
    void getPeers(const InfoHash& infoHash,
             const std::function<void(const std::vector<network::EndPoint> &)>
                 &callback) const;

    /**
     * @brief Announces a peer for an info hash
     * @param infoHash The info hash
     * @param port The port
     * @param callback The callback to call with the result
     */
    void announcePeer(const InfoHash& infoHash, uint16_t port,
                      const std::function<void(bool)> &callback) const;

    /**
     * @brief Pings a node
     * @param node The node to ping
     * @param callback The callback to call with the result
     */
    void ping(const std::shared_ptr<Node>& node, const std::function<void(bool)>& callback);

    /**
     * @brief Gets the crawler
     * @return The crawler
     */
    std::shared_ptr<Crawler> getCrawler() const;

    /**
     * @brief Gets the routing manager
     * @return The routing manager
     */
    std::shared_ptr<RoutingManager> getRoutingManager() const;

    /**
     * @brief Gets the peer storage
     * @return The peer storage
     */
    std::shared_ptr<PeerStorage> getPeerStorage() const;

private:
    /**
     * @brief Saves the routing table periodically
     */
    void saveRoutingTablePeriodically();

    /**
     * @brief Subscribes to events
     */
    void subscribeToEvents();

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
     * @brief Handles a system error event
     * @param event The event
     */
    void handleSystemErrorEvent(const std::shared_ptr<unified_event::Event>& event);

    NodeID m_nodeID;
    DHTConfig m_config;
    std::atomic<bool> m_running;
    std::shared_ptr<RoutingTable> m_routingTable;
    std::shared_ptr<SocketManager> m_socketManager;
    std::shared_ptr<MessageSender> m_messageSender;
    std::shared_ptr<MessageHandler> m_messageHandler;
    std::shared_ptr<bittorrent::BTMessageHandler> m_btMessageHandler;
    std::shared_ptr<TokenManager> m_tokenManager;
    std::shared_ptr<PeerStorage> m_peerStorage;
    std::shared_ptr<TransactionManager> m_transactionManager;
    std::shared_ptr<RoutingManager> m_routingManager;
    std::shared_ptr<NodeLookup> m_nodeLookup;
    std::shared_ptr<PeerLookup> m_peerLookup;
    std::shared_ptr<Bootstrapper> m_bootstrapper;
    std::shared_ptr<Crawler> m_crawler;

    // Event bus
    std::shared_ptr<unified_event::EventBus> m_eventBus;
    std::vector<int> m_eventSubscriptionIds;

    // Services
    std::shared_ptr<services::StatisticsService> m_statisticsService;

    // DHT extensions
    std::shared_ptr<extensions::MainlineDHT> m_mainlineDHT;
    std::shared_ptr<extensions::KademliaDHT> m_kademliaDHT;
    std::shared_ptr<extensions::AzureusDHT> m_azureusDHT;
    std::thread m_saveRoutingTableThread;
    std::mutex m_mutex;    // Logger removed
};

} // namespace dht_hunter::dht
