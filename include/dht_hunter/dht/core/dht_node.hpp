#pragma once

#include "dht_hunter/dht/core/dht_node_config.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/dht/core/dht_types.hpp"
#include "dht_hunter/dht/bootstrap/dht_bootstrapper.hpp"
#include "dht_hunter/dht/routing/dht_routing_manager.hpp"
#include "dht_hunter/dht/transactions/dht_transaction_types.hpp"
#include "dht_hunter/network/socket.hpp"
#include "dht_hunter/network/socket_factory.hpp"
#include "dht_hunter/network/udp_message_batcher.hpp"

// Forward declarations
namespace dht_hunter {
    namespace dht {
        class RoutingTable;
        class DHTSocketManager;
        class DHTMessageSender;
        class DHTMessageHandler;
        class DHTRoutingManager;
        class DHTNodeLookup;
        class DHTPeerLookup;
        class DHTPeerStorage;
        class DHTTokenManager;
        class DHTTransactionManager;
        class DHTPersistenceManager;
    }

    namespace crawler {
        class InfoHashCollector;
    }
}

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <chrono>
#include "dht_hunter/util/mutex_utils.hpp"

namespace dht_hunter::dht {

// Transaction callbacks are now defined in dht_transaction_types.hpp

/**
 * @brief Callback for node lookup completion
 * @param nodes The closest nodes found
 */
using NodeLookupCallback = std::function<void(const std::vector<std::shared_ptr<Node>>&)>;

/**
 * @brief Callback for get_peers lookup completion
 * @param peers The peers found
 * @param nodes The closest nodes found (if no peers were found)
 * @param token The token to use for announce_peer (if nodes were returned)
 */
using GetPeersLookupCallback = std::function<void(
    const std::vector<network::EndPoint>&,
    const std::vector<std::shared_ptr<Node>>&,
    const std::string&)>;

/**
 * @brief Represents a node in the DHT network
 *
 * This class is the main interface for interacting with the DHT network.
 * It delegates to specialized components for different aspects of DHT functionality.
 */
class DHTNode : public std::enable_shared_from_this<DHTNode> {
public:
    /**
     * @brief Constructs a DHT node
     * @param config The DHT node configuration
     * @param nodeID The node ID (optional, generated randomly if not provided)
     */
    explicit DHTNode(const DHTNodeConfig& config, const NodeID& nodeID = generateRandomNodeID());

    /**
     * @brief Constructs a DHT node (legacy constructor)
     * @param port The port to listen on
     * @param configDir The configuration directory to use
     * @param nodeID The node ID (optional, generated randomly if not provided)
     */
    explicit DHTNode(uint16_t port, const std::string& configDir = DEFAULT_CONFIG_DIR, const NodeID& nodeID = generateRandomNodeID());

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
     * @brief Gets the routing table
     * @return The routing table
     */
    const RoutingTable& getRoutingTable() const;

    /**
     * @brief Sets the InfoHash collector
     * @param collector The InfoHash collector
     */
    void setInfoHashCollector(std::shared_ptr<crawler::InfoHashCollector> collector);

    /**
     * @brief Gets the InfoHash collector
     * @return The InfoHash collector
     */
    std::shared_ptr<crawler::InfoHashCollector> getInfoHashCollector() const;

    /**
     * @brief Gets the message sender
     * @return The message sender
     */
    std::shared_ptr<DHTMessageSender> getMessageSender() const;

    /**
     * @brief Gets the message handler
     * @return The message handler
     */
    std::shared_ptr<DHTMessageHandler> getMessageHandler() const;

    /**
     * @brief Gets the transaction manager
     * @return The transaction manager
     */
    std::shared_ptr<DHTTransactionManager> getTransactionManager() const;

    /**
     * @brief Bootstraps the DHT node using a known node
     * @param endpoint The endpoint of the known node
     * @return True if the bootstrap was successful, false otherwise
     */
    bool bootstrap(const network::EndPoint& endpoint);

    /**
     * @brief Bootstraps the DHT node using a list of known nodes
     * @param endpoints The endpoints of the known nodes
     * @return True if at least one bootstrap was successful, false otherwise
     */
    bool bootstrap(const std::vector<network::EndPoint>& endpoints);

    /**
     * @brief Bootstraps the DHT node using the configured bootstrap nodes
     * @param config The bootstrapper configuration (optional, uses default if not provided)
     * @return True if the bootstrap was successful, false otherwise
     */
    bool bootstrapWithDefaultNodes(const DHTBootstrapperConfig& config = DHTBootstrapperConfig());

    /**
     * @brief Pings a node
     * @param endpoint The endpoint of the node to ping
     * @param responseCallback The callback to call when a response is received
     * @param errorCallback The callback to call when an error is received
     * @param timeoutCallback The callback to call when the transaction times out
     * @return True if the ping was sent successfully, false otherwise
     */
    bool ping(const network::EndPoint& endpoint,
             ResponseCallback responseCallback = nullptr,
             ErrorCallback errorCallback = nullptr,
             TimeoutCallback timeoutCallback = nullptr);

    /**
     * @brief Finds nodes close to a target ID
     * @param targetID The target ID
     * @param endpoint The endpoint of the node to query
     * @param responseCallback The callback to call when a response is received
     * @param errorCallback The callback to call when an error is received
     * @param timeoutCallback The callback to call when the transaction times out
     * @return True if the find_node was sent successfully, false otherwise
     */
    bool findNode(const NodeID& targetID,
                 const network::EndPoint& endpoint,
                 ResponseCallback responseCallback = nullptr,
                 ErrorCallback errorCallback = nullptr,
                 TimeoutCallback timeoutCallback = nullptr);

    /**
     * @brief Gets peers for an info hash
     * @param infoHash The info hash
     * @param endpoint The endpoint of the node to query
     * @param responseCallback The callback to call when a response is received
     * @param errorCallback The callback to call when an error is received
     * @param timeoutCallback The callback to call when the transaction times out
     * @return True if the get_peers was sent successfully, false otherwise
     */
    bool getPeers(const InfoHash& infoHash,
                 const network::EndPoint& endpoint,
                 ResponseCallback responseCallback = nullptr,
                 ErrorCallback errorCallback = nullptr,
                 TimeoutCallback timeoutCallback = nullptr);

    /**
     * @brief Announces as a peer for an info hash
     * @param infoHash The info hash
     * @param port The port to announce
     * @param token The token received from a previous get_peers query
     * @param endpoint The endpoint of the node to query
     * @param responseCallback The callback to call when a response is received
     * @param errorCallback The callback to call when an error is received
     * @param timeoutCallback The callback to call when the transaction times out
     * @return True if the announce_peer was sent successfully, false otherwise
     */
    bool announcePeer(const InfoHash& infoHash,
                     uint16_t port,
                     const std::string& token,
                     const network::EndPoint& endpoint,
                     ResponseCallback responseCallback = nullptr,
                     ErrorCallback errorCallback = nullptr,
                     TimeoutCallback timeoutCallback = nullptr);

    /**
     * @brief Performs an iterative node lookup to find the closest nodes to a target ID
     * @param targetID The target ID to look up
     * @param callback The callback to call when the lookup is complete
     * @return True if the lookup was started successfully, false otherwise
     */
    bool findClosestNodes(const NodeID& targetID, NodeLookupCallback callback);

    /**
     * @brief Performs an iterative lookup to find peers for an info hash
     * @param infoHash The info hash to look up
     * @param callback The callback to call when the lookup is complete
     * @return True if the lookup was started successfully, false otherwise
     */
    bool findPeers(const InfoHash& infoHash, GetPeersLookupCallback callback);

    /**
     * @brief Announces as a peer for an info hash to multiple nodes
     * @param infoHash The info hash
     * @param port The port to announce
     * @param callback The callback to call when the announcement is complete
     * @return True if the announcement was started successfully, false otherwise
     */
    bool announceAsPeer(const InfoHash& infoHash, uint16_t port, std::function<void(bool)> callback);

    /**
     * @brief Stores a peer for an info hash
     * @param infoHash The info hash
     * @param endpoint The peer's endpoint
     * @return True if the peer was stored, false otherwise
     */
    bool storePeer(const InfoHash& infoHash, const network::EndPoint& endpoint);

    /**
     * @brief Gets stored peers for an info hash
     * @param infoHash The info hash
     * @return The stored peers
     */
    std::vector<network::EndPoint> getStoredPeers(const InfoHash& infoHash) const;

    /**
     * @brief Gets the number of stored peers for an info hash
     * @param infoHash The info hash
     * @return The number of stored peers
     */
    size_t getStoredPeerCount(const InfoHash& infoHash) const;

    /**
     * @brief Gets the total number of stored peers across all info hashes
     * @return The total number of stored peers
     */
    size_t getTotalStoredPeerCount() const;

    /**
     * @brief Gets the number of info hashes with stored peers
     * @return The number of info hashes
     */
    size_t getInfoHashCount() const;

    /**
     * @brief Saves the routing table to a file
     * @param filePath The path to the file
     * @return True if the routing table was saved successfully, false otherwise
     */
    bool saveRoutingTable(const std::string& filePath) const;

    /**
     * @brief Loads the routing table from a file
     * @param filePath The path to the file
     * @return True if the routing table was loaded successfully, false otherwise
     */
    bool loadRoutingTable(const std::string& filePath);

    /**
     * @brief Loads the node ID from a file
     * @param filePath The path to the file
     * @return True if the node ID was loaded successfully, false otherwise
     */
    bool loadNodeID(const std::string& filePath);

    /**
     * @brief Saves the peer cache to a file
     * @param filePath The path to the file
     * @return True if the peer cache was saved successfully, false otherwise
     */
    bool savePeerCache(const std::string& filePath) const;

    /**
     * @brief Loads the peer cache from a file
     * @param filePath The path to the file
     * @return True if the peer cache was loaded successfully, false otherwise
     */
    bool loadPeerCache(const std::string& filePath);

    /**
     * @brief Saves the node ID to a file
     * @param filePath The path to the file
     * @return True if the node ID was saved successfully, false otherwise
     */
    bool saveNodeID(const std::string& filePath) const;

    /**
     * @brief Performs a lookup for a random node ID to discover more nodes
     * @return True if the lookup was started successfully, false otherwise
     */
    bool performRandomNodeLookup();

private:
    // Core components
    NodeID m_nodeID;
    uint16_t m_port;
    DHTNodeConfig m_config;
    std::atomic<bool> m_running{false};

    // Component managers
    std::shared_ptr<DHTSocketManager> m_socketManager;
    std::shared_ptr<DHTMessageSender> m_messageSender;
    std::shared_ptr<DHTMessageHandler> m_messageHandler;
    std::shared_ptr<DHTRoutingManager> m_routingManager;
    std::shared_ptr<DHTNodeLookup> m_nodeLookup;
    std::shared_ptr<DHTPeerLookup> m_peerLookup;
    std::shared_ptr<DHTPeerStorage> m_peerStorage;
    std::shared_ptr<DHTTokenManager> m_tokenManager;
    std::shared_ptr<DHTTransactionManager> m_transactionManager;
    std::shared_ptr<DHTPersistenceManager> m_persistenceManager;

    // Legacy components (to be removed once refactoring is complete)
    RoutingTable m_routingTable;
    std::string m_routingTablePath;
    std::string m_peerCachePath;
    std::string m_transactionsPath;

    // InfoHash collector
    std::shared_ptr<crawler::InfoHashCollector> m_infoHashCollector;
};

} // namespace dht_hunter::dht
