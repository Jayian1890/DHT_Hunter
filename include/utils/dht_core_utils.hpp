#pragma once

/**
 * @file dht_core_utils.hpp
 * @brief Consolidated DHT core utilities
 *
 * This file contains consolidated DHT core components including:
 * - DHT Node and Configuration
 * - Routing Table and KBucket
 * - Persistence Manager
 * - DHT Constants and Event Handlers
 */

// Standard library includes
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <random>
#include <algorithm>

// Project includes
#include "utils/common_utils.hpp"
#include "utils/domain_utils.hpp"
#include "utils/system_utils.hpp"
#include "dht_hunter/types/info_hash.hpp"
#include "dht_hunter/types/node_id.hpp"
#include "dht_hunter/types/endpoint.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"

// Forward declarations
namespace dht_hunter {
    namespace network {
        class SocketManager;
        class MessageSender;
        class MessageHandler;
    }

    namespace bittorrent {
        class BTMessageHandler;
    }

    namespace unified_event {
        class EventBus;
    }

    namespace dht {
        class MessageSender;
        class MessageHandler;
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
            class ExtensionFactory;
        }
    }
}

namespace dht_hunter::utils::dht_core {

//=============================================================================
// Constants
//=============================================================================

/**
 * @brief Maximum number of peers to store per info hash
 */
constexpr size_t MAX_PEERS_PER_INFOHASH = 100;

/**
 * @brief Time-to-live for stored peers in seconds (30 minutes)
 */
constexpr int PEER_TTL = 1800;

/**
 * @brief Interval for cleaning up expired peers in seconds (5 minutes)
 */
constexpr int PEER_CLEANUP_INTERVAL = 300;

/**
 * @brief Interval for saving the routing table in seconds (10 minutes)
 */
constexpr int ROUTING_TABLE_SAVE_INTERVAL = 600;

/**
 * @brief Default k-bucket size (maximum number of nodes in a bucket)
 */
constexpr size_t K_BUCKET_SIZE = 16;

/**
 * @brief Default MTU size for UDP packets
 */
constexpr size_t DEFAULT_MTU_SIZE = 1400;

/**
 * @brief Default bootstrap nodes
 */
constexpr const char* DEFAULT_BOOTSTRAP_NODES[] = {
    "dht.aelitis.com:6881",
    "dht.transmissionbt.com:6881",
    "dht.libtorrent.org:25401",
    "router.utorrent.com:6881"
};

/**
 * @brief Number of default bootstrap nodes
 */
constexpr size_t DEFAULT_BOOTSTRAP_NODES_COUNT = 4;

/**
 * @brief Default bootstrap timeout in seconds
 */
constexpr int DEFAULT_BOOTSTRAP_TIMEOUT = 10;

/**
 * @brief Default token rotation interval in seconds (5 minutes as per BEP-5)
 */
constexpr int DEFAULT_TOKEN_ROTATION_INTERVAL = 300;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @class DHTConfig
 * @brief Configuration for the DHT node
 */
class DHTConfig {
public:
    /**
     * @brief Constructor with default values
     */
    DHTConfig();

    /**
     * @brief Destructor
     */
    ~DHTConfig();

    /**
     * @brief Gets the UDP port
     * @return The UDP port
     */
    uint16_t getPort() const;

    /**
     * @brief Sets the UDP port
     * @param port The UDP port
     */
    void setPort(uint16_t port);

    /**
     * @brief Gets the k-bucket size
     * @return The k-bucket size
     */
    size_t getKBucketSize() const;

    /**
     * @brief Sets the k-bucket size
     * @param size The k-bucket size
     */
    void setKBucketSize(size_t size);

    /**
     * @brief Gets the bootstrap nodes
     * @return The bootstrap nodes
     */
    std::vector<std::string> getBootstrapNodes() const;

    /**
     * @brief Sets the bootstrap nodes
     * @param nodes The bootstrap nodes
     */
    void setBootstrapNodes(const std::vector<std::string>& nodes);

    /**
     * @brief Gets the bootstrap timeout
     * @return The bootstrap timeout in seconds
     */
    int getBootstrapTimeout() const;

    /**
     * @brief Sets the bootstrap timeout
     * @param timeout The bootstrap timeout in seconds
     */
    void setBootstrapTimeout(int timeout);

    /**
     * @brief Gets the token rotation interval
     * @return The token rotation interval in seconds
     */
    int getTokenRotationInterval() const;

    /**
     * @brief Sets the token rotation interval
     * @param interval The token rotation interval in seconds
     */
    void setTokenRotationInterval(int interval);

    /**
     * @brief Gets the MTU size
     * @return The MTU size
     */
    size_t getMTUSize() const;

    /**
     * @brief Sets the MTU size
     * @param size The MTU size
     */
    void setMTUSize(size_t size);

    /**
     * @brief Gets the config directory
     * @return The config directory
     */
    std::string getConfigDir() const;

    /**
     * @brief Sets the config directory
     * @param dir The config directory
     */
    void setConfigDir(const std::string& dir);

private:
    uint16_t m_port;
    size_t m_kBucketSize;
    std::vector<std::string> m_bootstrapNodes;
    int m_bootstrapTimeout;
    int m_tokenRotationInterval;
    size_t m_mtuSize;
    std::string m_configDir;
};

//=============================================================================
// Routing Table
//=============================================================================

/**
 * @class KBucket
 * @brief A k-bucket in the routing table
 */
class KBucket {
public:
    /**
     * @brief Constructor
     * @param prefix The prefix length
     * @param kSize The maximum number of nodes in the bucket
     */
    KBucket(size_t prefix, size_t kSize);

    /**
     * @brief Copy constructor (deleted)
     */
    KBucket(const KBucket&) = delete;

    /**
     * @brief Move constructor
     */
    KBucket(KBucket&& other) noexcept;

    /**
     * @brief Copy assignment operator (deleted)
     */
    KBucket& operator=(const KBucket&) = delete;

    /**
     * @brief Move assignment operator
     */
    KBucket& operator=(KBucket&& other) noexcept;

    /**
     * @brief Adds a node to the bucket
     * @param node The node to add
     * @return True if the node was added, false otherwise
     */
    bool addNode(std::shared_ptr<types::Node> node);

    /**
     * @brief Removes a node from the bucket
     * @param nodeID The ID of the node to remove
     * @return True if the node was removed, false otherwise
     */
    bool removeNode(const types::NodeID& nodeID);

    /**
     * @brief Gets all nodes in the bucket
     * @return The nodes in the bucket
     */
    std::vector<std::shared_ptr<types::Node>> getNodes() const;

    /**
     * @brief Checks if the bucket contains a node ID
     * @param nodeID The node ID to check
     * @param ownID The own node ID
     * @return True if the bucket contains the node ID, false otherwise
     */
    bool containsNodeID(const types::NodeID& nodeID, const types::NodeID& ownID) const;

    /**
     * @brief Gets the prefix length
     * @return The prefix length
     */
    size_t getPrefix() const;

    /**
     * @brief Gets the last changed time
     * @return The last changed time
     */
    std::chrono::steady_clock::time_point getLastChanged() const;

    /**
     * @brief Updates the last changed time
     */
    void updateLastChanged();

private:
    size_t m_prefix;
    size_t m_kSize;
    std::vector<std::shared_ptr<types::Node>> m_nodes;
    std::chrono::steady_clock::time_point m_lastChanged;
    mutable std::mutex m_mutex;
};

/**
 * @class RoutingTable
 * @brief The DHT routing table
 */
class RoutingTable {
public:
    /**
     * @brief Gets the singleton instance
     * @param ownID The own node ID
     * @param kBucketSize The k-bucket size
     * @return The singleton instance
     */
    static std::shared_ptr<RoutingTable> getInstance(
        const types::NodeID& ownID,
        size_t kBucketSize = K_BUCKET_SIZE);

    /**
     * @brief Destructor
     */
    ~RoutingTable();

    /**
     * @brief Adds a node to the routing table
     * @param node The node to add
     * @return True if the node was added, false otherwise
     */
    bool addNode(const std::shared_ptr<types::Node>& node);

    /**
     * @brief Removes a node from the routing table
     * @param nodeID The ID of the node to remove
     * @return True if the node was removed, false otherwise
     */
    bool removeNode(const types::NodeID& nodeID);

    /**
     * @brief Gets the closest nodes to a target ID
     * @param targetID The target ID
     * @param k The maximum number of nodes to return
     * @return The closest nodes
     */
    std::vector<std::shared_ptr<types::Node>> getClosestNodes(
        const types::NodeID& targetID,
        size_t k) const;

    /**
     * @brief Gets all nodes in the routing table
     * @return All nodes in the routing table
     */
    std::vector<std::shared_ptr<types::Node>> getAllNodes() const;

    /**
     * @brief Gets the number of nodes in the routing table
     * @return The number of nodes
     */
    size_t getNodeCount() const;

    /**
     * @brief Gets the number of buckets in the routing table
     * @return The number of buckets
     */
    size_t getBucketCount() const;

    /**
     * @brief Saves the routing table to a file
     * @param filePath The file path
     * @return True if the routing table was saved, false otherwise
     */
    bool saveToFile(const std::string& filePath) const;

    /**
     * @brief Loads the routing table from a file
     * @param filePath The file path
     * @return True if the routing table was loaded, false otherwise
     */
    bool loadFromFile(const std::string& filePath);

private:
    /**
     * @brief Constructor
     * @param ownID The own node ID
     * @param kBucketSize The k-bucket size
     */
    RoutingTable(const types::NodeID& ownID, size_t kBucketSize);

    /**
     * @brief Gets the bucket for a node ID
     * @param nodeID The node ID
     * @return The bucket
     */
    KBucket& getBucketForNodeID(const types::NodeID& nodeID);

    /**
     * @brief Splits a bucket
     * @param bucketIndex The bucket index
     */
    void splitBucket(size_t bucketIndex);

    // Static instance for singleton pattern
    static std::shared_ptr<RoutingTable> s_instance;
    static std::mutex s_instanceMutex;

    types::NodeID m_ownID;
    size_t m_kBucketSize;
    std::vector<KBucket> m_buckets;
    std::shared_ptr<unified_event::EventBus> m_eventBus;
    mutable std::mutex m_mutex;
};

//=============================================================================
// DHT Node
//=============================================================================

/**
 * @class DHTNode
 * @brief The DHT node
 */
class DHTNode {
public:
    /**
     * @brief Gets the singleton instance
     * @param config The DHT configuration
     * @param nodeID The node ID
     * @return The singleton instance
     */
    static std::shared_ptr<DHTNode> getInstance(
        const DHTConfig& config = DHTConfig(),
        const types::NodeID& nodeID = types::NodeID::random());

    /**
     * @brief Destructor
     */
    ~DHTNode();

    /**
     * @brief Starts the DHT node
     * @return True if the node was started, false otherwise
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
    types::NodeID getNodeID() const;

    /**
     * @brief Gets the DHT configuration
     * @return The DHT configuration
     */
    DHTConfig getConfig() const;

    /**
     * @brief Gets the routing table
     * @return The routing table
     */
    std::shared_ptr<RoutingTable> getRoutingTable() const;

    /**
     * @brief Finds nodes close to a target ID
     * @param targetID The target ID
     * @param callback The callback function
     */
    void findNode(
        const types::NodeID& targetID,
        std::function<void(const std::vector<std::shared_ptr<types::Node>>&)> callback) const;

    /**
     * @brief Gets peers for an info hash
     * @param infoHash The info hash
     * @param callback The callback function
     */
    void getPeers(
        const types::InfoHash& infoHash,
        std::function<void(const std::vector<network::EndPoint>&)> callback) const;

    /**
     * @brief Announces a peer for an info hash
     * @param infoHash The info hash
     * @param port The port
     * @param callback The callback function
     */
    void announcePeer(
        const types::InfoHash& infoHash,
        uint16_t port,
        std::function<void(bool)> callback) const;

public:
    /**
     * @brief Constructor
     * @param config The DHT configuration
     * @param nodeID The node ID
     */
    DHTNode(const DHTConfig& config, const types::NodeID& nodeID);
    /**
     * @brief Sends a query to a node
     * @param nodeId The node ID
     * @param query The query message
     * @param callback The callback function
     * @return True if the query was sent, false otherwise
     */
    bool sendQueryToNode(
        const types::NodeID& nodeId,
        std::shared_ptr<dht::QueryMessage> query,
        std::function<void(std::shared_ptr<dht::ResponseMessage>, bool)> callback);

    /**
     * @brief Finds nodes that might have metadata for an info hash
     * @param infoHash The info hash
     * @param callback The callback function
     */
    void findNodesWithMetadata(
        const types::InfoHash& infoHash,
        std::function<void(const std::vector<std::shared_ptr<dht::Node>>&)> callback);

private:

    // Static instance for singleton pattern
    static std::shared_ptr<DHTNode> s_instance;
    static std::mutex s_instanceMutex;

    types::NodeID m_nodeID;
    DHTConfig m_config;
    std::atomic<bool> m_running;
    std::shared_ptr<RoutingTable> m_routingTable;
    std::shared_ptr<network::SocketManager> m_socketManager;
    std::shared_ptr<dht::MessageSender> m_messageSender;
    std::shared_ptr<dht::MessageHandler> m_messageHandler;
    std::shared_ptr<bittorrent::BTMessageHandler> m_btMessageHandler;
    std::shared_ptr<dht::TokenManager> m_tokenManager;
    std::shared_ptr<dht::PeerStorage> m_peerStorage;
    std::shared_ptr<dht::TransactionManager> m_transactionManager;
    std::shared_ptr<dht::RoutingManager> m_routingManager;
    std::shared_ptr<dht::NodeLookup> m_nodeLookup;
    std::shared_ptr<dht::PeerLookup> m_peerLookup;
    std::shared_ptr<dht::Bootstrapper> m_bootstrapper;
    std::shared_ptr<dht::Crawler> m_crawler;
    std::shared_ptr<unified_event::EventBus> m_eventBus;
    std::unordered_map<std::string, std::shared_ptr<dht::extensions::DHTExtension>> m_extensions;
};

//=============================================================================
// Persistence Manager
//=============================================================================

/**
 * @class PersistenceManager
 * @brief Manages persistence of DHT state
 */
class PersistenceManager {
public:
    /**
     * @brief Gets the singleton instance
     * @param configDir The configuration directory
     * @return The singleton instance
     */
    static std::shared_ptr<PersistenceManager> getInstance(const std::string& configDir = ".");

    /**
     * @brief Destructor
     */
    ~PersistenceManager();

    /**
     * @brief Starts the persistence manager
     * @param routingTable The routing table
     * @param peerStorage The peer storage
     * @return True if the persistence manager was started, false otherwise
     */
    bool start(
        std::shared_ptr<RoutingTable> routingTable,
        std::shared_ptr<dht::PeerStorage> peerStorage);

    /**
     * @brief Stops the persistence manager
     */
    void stop();

    /**
     * @brief Saves the routing table
     * @return True if the routing table was saved, false otherwise
     */
    bool saveRoutingTable();

    /**
     * @brief Loads the routing table
     * @return True if the routing table was loaded, false otherwise
     */
    bool loadRoutingTable();

    /**
     * @brief Saves the peer storage
     * @return True if the peer storage was saved, false otherwise
     */
    bool savePeerStorage();

    /**
     * @brief Loads the peer storage
     * @return True if the peer storage was loaded, false otherwise
     */
    bool loadPeerStorage();

    /**
     * @brief Saves a node ID to a file
     * @param nodeID The node ID
     * @return True if the node ID was saved, false otherwise
     */
    bool saveNodeID(const types::NodeID& nodeID);

    /**
     * @brief Loads a node ID from a file
     * @return The node ID, or a random node ID if the file doesn't exist
     */
    types::NodeID loadNodeID();

private:
    /**
     * @brief Constructor
     * @param configDir The configuration directory
     */
    PersistenceManager(const std::string& configDir);

    /**
     * @brief Save thread function
     */
    void saveThreadFunc();

    // Static instance for singleton pattern
    static std::shared_ptr<PersistenceManager> s_instance;
    static std::mutex s_instanceMutex;

    std::string m_configDir;
    std::string m_routingTablePath;
    std::string m_peerStoragePath;
    std::string m_metadataPath;
    std::string m_nodeIDPath;
    std::atomic<bool> m_running;
    std::thread m_saveThread;
    mutable std::mutex m_mutex;
    std::shared_ptr<RoutingTable> m_routingTable;
    std::shared_ptr<dht::PeerStorage> m_peerStorage;
};

} // namespace dht_hunter::utils::dht_core

// Forward declarations for DHT components
namespace dht_hunter::dht {
    class SocketManager;
    class MessageSender;
    class MessageHandler;
    class QueryMessage;
    class ResponseMessage;
    class ErrorMessage;
    class TokenManager;
    class PeerStorage;
    class TransactionManager;
    class RoutingManager;
    class NodeLookup;
    class PeerLookup;
    class Bootstrapper;
    class Crawler;
}

// Backward compatibility
namespace dht_hunter::dht {
    using dht_hunter::utils::dht_core::DHTConfig;
    using dht_hunter::utils::dht_core::KBucket;
    using dht_hunter::utils::dht_core::RoutingTable;
    using dht_hunter::utils::dht_core::DHTNode;
    using dht_hunter::utils::dht_core::PersistenceManager;

    // Constants
    using dht_hunter::utils::dht_core::MAX_PEERS_PER_INFOHASH;
    using dht_hunter::utils::dht_core::PEER_TTL;
    using dht_hunter::utils::dht_core::PEER_CLEANUP_INTERVAL;
    using dht_hunter::utils::dht_core::ROUTING_TABLE_SAVE_INTERVAL;
    using dht_hunter::utils::dht_core::K_BUCKET_SIZE;
    using dht_hunter::utils::dht_core::DEFAULT_MTU_SIZE;
    using dht_hunter::utils::dht_core::DEFAULT_BOOTSTRAP_NODES;
    using dht_hunter::utils::dht_core::DEFAULT_BOOTSTRAP_NODES_COUNT;
    using dht_hunter::utils::dht_core::DEFAULT_BOOTSTRAP_TIMEOUT;
    using dht_hunter::utils::dht_core::DEFAULT_TOKEN_ROTATION_INTERVAL;
}
