#pragma once

#include "dht_hunter/dht/types.hpp"
#include "dht_hunter/dht/routing_table.hpp"
#include "dht_hunter/dht/message.hpp"
#include "dht_hunter/dht/query_message.hpp"
#include "dht_hunter/dht/response_message.hpp"
#include "dht_hunter/dht/error_message.hpp"
#include "dht_hunter/dht/transaction_persistence.hpp"
#include "dht_hunter/network/socket.hpp"
#include "dht_hunter/network/socket_factory.hpp"
#include "dht_hunter/network/udp_message_batcher.hpp"

// Forward declaration
namespace dht_hunter::crawler {
    class InfoHashCollector;
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

/**
 * @brief Maximum number of concurrent transactions
 */
constexpr size_t MAX_TRANSACTIONS = 1024; // Increased from 256 to 1024

/**
 * @brief Transaction timeout in seconds
 */
constexpr int TRANSACTION_TIMEOUT = 30; // Increased from 15 to 30 seconds to give more time for bootstrap

/**
 * @brief Default alpha parameter for parallel lookups (number of concurrent requests)
 */
constexpr size_t DEFAULT_LOOKUP_ALPHA = 5;

/**
 * @brief Default maximum number of nodes to store in a lookup result
 */
constexpr size_t DEFAULT_LOOKUP_MAX_RESULTS = 16;

/**
 * @brief Maximum number of iterations for a node lookup
 */
constexpr size_t LOOKUP_MAX_ITERATIONS = 20;

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
 * @brief Interval for saving transactions in seconds (5 minutes)
 */
constexpr int TRANSACTION_SAVE_INTERVAL = 300;

/**
 * @brief Default configuration directory
 */
constexpr char DEFAULT_CONFIG_DIR[] = "config";

/**
 * @brief Default setting for whether to save the routing table after each new node is added
 */
constexpr bool DEFAULT_SAVE_ROUTING_TABLE_ON_NEW_NODE = true;

/**
 * @brief Default setting for whether to save transactions on shutdown
 */
constexpr bool DEFAULT_SAVE_TRANSACTIONS_ON_SHUTDOWN = true;

/**
 * @brief Default setting for whether to load transactions on startup
 */
constexpr bool DEFAULT_LOAD_TRANSACTIONS_ON_STARTUP = true;

/**
 * @brief Configuration for the DHT node
 */
struct DHTNodeConfig {
    uint16_t port = 6881;                                  ///< Port to listen on
    std::string configDir = DEFAULT_CONFIG_DIR;           ///< Configuration directory
    size_t kBucketSize = DEFAULT_LOOKUP_MAX_RESULTS;      ///< Maximum number of nodes in a k-bucket
    size_t lookupAlpha = DEFAULT_LOOKUP_ALPHA;            ///< Alpha parameter for parallel lookups
    size_t lookupMaxResults = DEFAULT_LOOKUP_MAX_RESULTS; ///< Maximum number of nodes to store in a lookup result
    bool saveRoutingTableOnNewNode = DEFAULT_SAVE_ROUTING_TABLE_ON_NEW_NODE; ///< Whether to save the routing table after each new node is added
    bool saveTransactionsOnShutdown = DEFAULT_SAVE_TRANSACTIONS_ON_SHUTDOWN; ///< Whether to save transactions on shutdown
    bool loadTransactionsOnStartup = DEFAULT_LOAD_TRANSACTIONS_ON_STARTUP;   ///< Whether to load transactions on startup
    std::string transactionsPath = "";                     ///< Path to the transactions file (empty = auto-generate)
};

// Default directory for configuration files is defined above

/**
 * @brief Default path for the routing table file
 */
constexpr const char* DEFAULT_ROUTING_TABLE_PATH = "config/routing_table.dat";

/**
 * @brief Callback for DHT responses
 */
using ResponseCallback = std::function<void(std::shared_ptr<ResponseMessage>)>;

/**
 * @brief Callback for DHT errors
 */
using ErrorCallback = std::function<void(std::shared_ptr<ErrorMessage>)>;

/**
 * @brief Callback for DHT timeouts
 */
using TimeoutCallback = std::function<void()>;

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
 * @brief Represents a transaction in the DHT network
 */
struct Transaction {
    TransactionID id;
    std::chrono::steady_clock::time_point timestamp;
    ResponseCallback responseCallback;
    ErrorCallback errorCallback;
    TimeoutCallback timeoutCallback;
};

/**
 * @brief Represents the state of a node during a lookup
 */
struct LookupNode {
    std::shared_ptr<Node> node;            ///< The node
    bool queried;                          ///< Whether the node has been queried
    bool responded;                        ///< Whether the node has responded
    NodeID distance;                       ///< Distance to the target ID
};

/**
 * @brief Represents a node lookup operation
 */
struct NodeLookup {
    NodeID targetID;                       ///< Target ID to look up
    std::vector<LookupNode> nodes;         ///< Nodes being considered in the lookup
    size_t activeQueries;                  ///< Number of active queries
    size_t iterations;                     ///< Number of iterations performed
    NodeLookupCallback callback;           ///< Callback for lookup completion
    util::CheckedMutex mutex;              ///< Mutex for thread safety
    bool completed;                        ///< Whether the lookup has completed
};

/**
 * @brief Represents a get_peers lookup operation
 */
struct GetPeersLookup {
    InfoHash infoHash;                     ///< Info hash to look up
    std::vector<LookupNode> nodes;         ///< Nodes being considered in the lookup
    std::vector<network::EndPoint> peers;  ///< Peers found so far
    std::string token;                     ///< Token for announce_peer (from the closest node)
    size_t activeQueries;                  ///< Number of active queries
    size_t iterations;                     ///< Number of iterations performed
    GetPeersLookupCallback callback;       ///< Callback for lookup completion
    util::CheckedMutex mutex;              ///< Mutex for thread safety
    bool completed;                        ///< Whether the lookup has completed
};

/**
 * @brief Represents a stored peer with timestamp
 */
struct StoredPeer {
    network::EndPoint endpoint;            ///< Peer endpoint (IP and port)
    std::chrono::steady_clock::time_point timestamp; ///< When the peer was added or updated
    bool operator==(const StoredPeer& other) const {
        return endpoint == other.endpoint;
    }
};

/**
 * @brief Hash function for StoredPeer
 */
struct StoredPeerHash {
    size_t operator()(const StoredPeer& peer) const {
        return std::hash<std::string>()(peer.endpoint.toString());
    }
};

/**
 * @brief Equality function for StoredPeer
 */
struct StoredPeerEqual {
    bool operator()(const StoredPeer& lhs, const StoredPeer& rhs) const {
        return lhs.endpoint == rhs.endpoint;
    }
};

/**
 * @brief Represents a node in the DHT network
 */
class DHTNode {
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

private:
    /**
     * @brief Sends a query message
     * @param query The query message
     * @param endpoint The endpoint to send the query to
     * @param responseCallback The callback to call when a response is received
     * @param errorCallback The callback to call when an error is received
     * @param timeoutCallback The callback to call when the transaction times out
     * @return True if the query was sent successfully, false otherwise
     */
    bool sendQuery(std::shared_ptr<QueryMessage> query,
                  const network::EndPoint& endpoint,
                  ResponseCallback responseCallback = nullptr,
                  ErrorCallback errorCallback = nullptr,
                  TimeoutCallback timeoutCallback = nullptr);

    /**
     * @brief Handles a received message
     * @param message The received message
     * @param sender The sender's endpoint
     */
    void handleMessage(std::shared_ptr<Message> message, const network::EndPoint& sender);

    /**
     * @brief Handles a query message
     * @param query The query message
     * @param sender The sender's endpoint
     */
    void handleQuery(std::shared_ptr<QueryMessage> query, const network::EndPoint& sender);

    /**
     * @brief Handles a response message
     * @param response The response message
     * @param sender The sender's endpoint
     */
    void handleResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender);

    /**
     * @brief Handles an error message
     * @param error The error message
     * @param sender The sender's endpoint
     */
    void handleError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender);

    /**
     * @brief Handles a ping query
     * @param query The ping query
     * @param sender The sender's endpoint
     */
    void handlePingQuery(std::shared_ptr<PingQuery> query, const network::EndPoint& sender);

    /**
     * @brief Handles a find_node query
     * @param query The find_node query
     * @param sender The sender's endpoint
     */
    void handleFindNodeQuery(std::shared_ptr<FindNodeQuery> query, const network::EndPoint& sender);

    /**
     * @brief Handles a get_peers query
     * @param query The get_peers query
     * @param sender The sender's endpoint
     */
    void handleGetPeersQuery(std::shared_ptr<GetPeersQuery> query, const network::EndPoint& sender);

    /**
     * @brief Handles an announce_peer query
     * @param query The announce_peer query
     * @param sender The sender's endpoint
     */
    void handleAnnouncePeerQuery(std::shared_ptr<AnnouncePeerQuery> query, const network::EndPoint& sender);

    /**
     * @brief Sends a response message
     * @param response The response message
     * @param endpoint The endpoint to send the response to
     * @return True if the response was sent successfully, false otherwise
     */
    bool sendResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& endpoint);

    /**
     * @brief Sends an error message
     * @param error The error message
     * @param endpoint The endpoint to send the error to
     * @return True if the error was sent successfully, false otherwise
     */
    bool sendError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& endpoint);

    /**
     * @brief Adds a transaction
     * @param transaction The transaction to add
     * @return True if the transaction was added, false otherwise
     */
    bool addTransaction(const Transaction& transaction);

    /**
     * @brief Removes a transaction
     * @param id The transaction ID
     * @return True if the transaction was removed, false otherwise
     */
    bool removeTransaction(const TransactionID& id);

    /**
     * @brief Finds a transaction
     * @param id The transaction ID
     * @return The transaction, or nullptr if not found
     */
    std::shared_ptr<Transaction> findTransaction(const TransactionID& id);

    /**
     * @brief Checks for timed out transactions
     */
    void checkTimeouts();

    /**
     * @brief Processes the transaction queue
     */
    void processTransactionQueue();

public:
    /**
     * @brief Performs a lookup for a random node ID to discover more nodes
     * @return True if the lookup was started successfully, false otherwise
     */
    bool performRandomNodeLookup();

    /**
     * @brief Receives messages
     */
    void receiveMessages();

    /**
     * @brief Adds a node to the routing table
     * @param id The node ID
     * @param endpoint The node's endpoint
     * @return True if the node was added, false otherwise
     */
    bool addNode(const NodeID& id, const network::EndPoint& endpoint);

    /**
     * @brief Generates a token for a node
     * @param endpoint The node's endpoint
     * @return The token
     */
    std::string generateToken(const network::EndPoint& endpoint);

    /**
     * @brief Validates a token for a node
     * @param token The token
     * @param endpoint The node's endpoint
     * @return True if the token is valid, false otherwise
     */
    bool validateToken(const std::string& token, const network::EndPoint& endpoint);

    /**
     * @brief Starts a node lookup operation
     * @param targetID The target ID to look up
     * @param callback The callback to call when the lookup is complete
     * @return True if the lookup was started successfully, false otherwise
     */
    bool startNodeLookup(const NodeID& targetID, NodeLookupCallback callback);

    /**
     * @brief Continues a node lookup operation
     * @param lookup The lookup operation
     */
    void continueNodeLookup(std::shared_ptr<NodeLookup> lookup);

    /**
     * @brief Handles a find_node response during a node lookup
     * @param lookup The lookup operation
     * @param response The find_node response
     * @param endpoint The endpoint that sent the response
     */
    void handleNodeLookupResponse(std::shared_ptr<NodeLookup> lookup,
                                 std::shared_ptr<FindNodeResponse> response,
                                 const network::EndPoint& endpoint);

    /**
     * @brief Starts a get_peers lookup operation
     * @param infoHash The info hash to look up
     * @param callback The callback to call when the lookup is complete
     * @return True if the lookup was started successfully, false otherwise
     */
    bool startGetPeersLookup(const InfoHash& infoHash, GetPeersLookupCallback callback);

    /**
     * @brief Continues a get_peers lookup operation
     * @param lookup The lookup operation
     */
    void continueGetPeersLookup(std::shared_ptr<GetPeersLookup> lookup);

    /**
     * @brief Handles a get_peers response during a get_peers lookup
     * @param lookup The lookup operation
     * @param response The get_peers response
     * @param endpoint The endpoint that sent the response
     */
    void handleGetPeersLookupResponse(std::shared_ptr<GetPeersLookup> lookup,
                                     std::shared_ptr<GetPeersResponse> response,
                                     const network::EndPoint& endpoint);

    /**
     * @brief Completes a node lookup operation
     * @param lookup The lookup operation
     */
    void completeNodeLookup(std::shared_ptr<NodeLookup> lookup);

    /**
     * @brief Completes a get_peers lookup operation
     * @param lookup The lookup operation
     */
    void completeGetPeersLookup(std::shared_ptr<GetPeersLookup> lookup);

    /**
     * @brief Cleans up expired peers
     */
    void cleanupExpiredPeers();

    /**
     * @brief Thread function for cleaning up expired peers
     */
    void peerCleanupThread();

    /**
     * @brief Thread function for periodically saving the routing table
     */
    void routingTableSaveThread();

    /**
     * @brief Saves transactions to a file
     * @param filePath The path to the file
     * @return True if the transactions were saved successfully, false otherwise
     */
    bool saveTransactions(const std::string& filePath) const;

    /**
     * @brief Loads transactions from a file
     * @param filePath The path to the file
     * @return True if the transactions were loaded successfully, false otherwise
     */
    bool loadTransactions(const std::string& filePath);

    /**
     * @brief Thread function for periodically saving transactions
     */
    void transactionSaveThread();

    NodeID m_nodeID;
    uint16_t m_port;
    RoutingTable m_routingTable;
    std::unique_ptr<network::Socket> m_socket;
    util::CheckedMutex m_socketMutex; // Mutex for socket access
    std::unique_ptr<network::UDPMessageBatcher> m_messageBatcher;
    std::string m_routingTablePath;
    std::string m_peerCachePath;
    std::string m_transactionsPath;
    DHTNodeConfig m_config; // Configuration for the DHT node

    // InfoHash collector
    std::shared_ptr<crawler::InfoHashCollector> m_infoHashCollector;

    // Peer storage
    std::unordered_map<std::string, std::unordered_set<StoredPeer, StoredPeerHash, StoredPeerEqual>> m_peers;
    mutable util::CheckedMutex m_peersLock;
    std::thread m_peerCleanupThread;
    std::thread m_routingTableSaveThread;
    std::thread m_transactionSaveThread;

    std::unordered_map<std::string, std::shared_ptr<Transaction>> m_transactions;
    std::queue<std::shared_ptr<Transaction>> m_transactionQueue; // Queue for pending transactions
    util::CheckedMutex m_transactionsMutex;
    size_t m_maxQueuedTransactions = 2048; // Maximum number of queued transactions

    std::unordered_map<std::string, std::shared_ptr<NodeLookup>> m_nodeLookups;
    util::CheckedMutex m_nodeLookupsLock;

    std::unordered_map<std::string, std::shared_ptr<GetPeersLookup>> m_getPeersLookups;
    util::CheckedMutex m_getPeersLookupsLock;

    std::atomic<bool> m_running{false};
    std::thread m_receiveThread;
    std::thread m_timeoutThread;

    // Token generation and validation
    std::string m_secret;
    std::chrono::steady_clock::time_point m_secretLastChanged;
    std::string m_previousSecret;
    util::CheckedMutex m_secretMutex;
};

} // namespace dht_hunter::dht
