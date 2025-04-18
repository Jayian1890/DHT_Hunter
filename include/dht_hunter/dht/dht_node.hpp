#pragma once

#include "dht_hunter/dht/types.hpp"
#include "dht_hunter/dht/routing_table.hpp"
#include "dht_hunter/dht/message.hpp"
#include "dht_hunter/dht/query_message.hpp"
#include "dht_hunter/dht/response_message.hpp"
#include "dht_hunter/dht/error_message.hpp"
#include "dht_hunter/network/socket.hpp"
#include "dht_hunter/network/socket_factory.hpp"

#include <memory>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <chrono>

namespace dht_hunter::dht {

/**
 * @brief Maximum number of concurrent transactions
 */
constexpr size_t MAX_TRANSACTIONS = 256;

/**
 * @brief Transaction timeout in seconds
 */
constexpr int TRANSACTION_TIMEOUT = 15;

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
 * @brief Represents a node in the DHT network
 */
class DHTNode {
public:
    /**
     * @brief Constructs a DHT node
     * @param port The port to listen on
     * @param nodeID The node ID (optional, generated randomly if not provided)
     */
    explicit DHTNode(uint16_t port, const NodeID& nodeID = generateRandomNodeID());
    
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
    
    NodeID m_nodeID;
    uint16_t m_port;
    RoutingTable m_routingTable;
    std::unique_ptr<network::Socket> m_socket;
    
    std::unordered_map<std::string, std::shared_ptr<Transaction>> m_transactions;
    std::mutex m_transactionsMutex;
    
    std::atomic<bool> m_running;
    std::thread m_receiveThread;
    std::thread m_timeoutThread;
    
    // Token generation and validation
    std::string m_secret;
    std::chrono::steady_clock::time_point m_secretLastChanged;
    std::string m_previousSecret;
    std::mutex m_secretMutex;
};

} // namespace dht_hunter::dht
