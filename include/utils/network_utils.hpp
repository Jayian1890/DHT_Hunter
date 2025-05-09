#pragma once

/**
 * @file network_utils.hpp
 * @brief Consolidated network utilities
 *
 * This file contains consolidated network components including:
 * - UDP socket, server, and client
 * - HTTP server and client
 * - Network address and endpoint
 * - NAT traversal services
 * - DHT-specific network components (SocketManager, MessageSender, MessageHandler)
 * - Transaction management
 */

// Standard library includes
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <map>
#include <unordered_map>
#include <sys/types.h>  // for ssize_t
#include <chrono>
#include <random>

// Project includes
#include "utils/common_utils.hpp"
#include "utils/system_utils.hpp"
#include "utils/dht_core_utils.hpp"
#include "dht_hunter/types/endpoint.hpp"
#include "dht_hunter/types/node_id.hpp"
#include "dht_hunter/types/info_hash.hpp"

// Forward declarations
namespace dht_hunter {
    namespace utils {
        namespace dht_core {
            class DHTConfig;
            class RoutingTable;
        }
        namespace common {
            class BencodeValue;
        }
    }
    namespace types {
        class NodeID;
        class InfoHash;
        class EndPoint;
    }
}

namespace dht_hunter::utils::network {

// Namespace aliases for convenience
namespace dht_core = dht_hunter::utils::dht_core;
namespace common = dht_hunter::utils::common;
namespace types = dht_hunter::types;

//=============================================================================
// Constants
//=============================================================================

/**
 * @brief Default MTU size for UDP packets
 */
constexpr size_t DEFAULT_MTU_SIZE = 1400;

/**
 * @brief Default UDP port
 */
constexpr uint16_t DEFAULT_UDP_PORT = 6881;

/**
 * @brief Default HTTP port
 */
constexpr uint16_t DEFAULT_HTTP_PORT = 8080;

/**
 * @brief Default socket timeout in milliseconds
 */
constexpr int DEFAULT_SOCKET_TIMEOUT_MS = 1000;

//=============================================================================
// UDP Socket
//=============================================================================

/**
 * @class UDPSocket
 * @brief A simple UDP socket implementation for sending and receiving datagrams.
 */
class UDPSocket {
public:
    /**
     * @brief Default constructor.
     */
    UDPSocket();

    /**
     * @brief Destructor.
     */
    ~UDPSocket();

    /**
     * @brief Bind the socket to a specific port.
     * @param port The port to bind to.
     * @return True if successful, false otherwise.
     */
    bool bind(uint16_t port);

    /**
     * @brief Send data to a specific address and port.
     * @param data The data to send.
     * @param size The size of the data.
     * @param address The destination address.
     * @param port The destination port.
     * @return The number of bytes sent, or -1 on error.
     */
    ssize_t sendTo(const void* data, size_t size, const std::string& address, uint16_t port);

    /**
     * @brief Receive data from any address and port.
     * @param buffer The buffer to receive data into.
     * @param size The size of the buffer.
     * @param address The source address (output).
     * @param port The source port (output).
     * @return The number of bytes received, or -1 on error.
     */
    ssize_t receiveFrom(void* buffer, size_t size, std::string& address, uint16_t& port);

    /**
     * @brief Start the receive loop.
     * @return True if successful, false otherwise.
     */
    bool startReceiveLoop();

    /**
     * @brief Stop the receive loop.
     */
    void stopReceiveLoop();

    /**
     * @brief Set a callback function to be called when data is received.
     * @param callback The callback function.
     */
    void setReceiveCallback(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback);

    /**
     * @brief Check if the socket is valid.
     * @return True if the socket is valid, false otherwise.
     */
    bool isValid() const;

    /**
     * @brief Close the socket.
     */
    void close();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

//=============================================================================
// UDP Server
//=============================================================================

/**
 * @class UDPServer
 * @brief A simple UDP server that listens for incoming datagrams.
 */
class UDPServer {
public:
    /**
     * @brief Default constructor.
     */
    UDPServer();

    /**
     * @brief Destructor.
     */
    ~UDPServer();

    /**
     * @brief Start the server on the specified port.
     * @param port The port to listen on.
     * @return True if successful, false otherwise.
     */
    bool start(uint16_t port);

    /**
     * @brief Start the server using the port from the configuration.
     * @return True if successful, false otherwise.
     */
    bool start();

    /**
     * @brief Stop the server.
     */
    void stop();

    /**
     * @brief Check if the server is running.
     * @return True if the server is running, false otherwise.
     */
    bool isRunning() const;

    /**
     * @brief Send data to a specific client.
     * @param data The data to send.
     * @param address The client's address.
     * @param port The client's port.
     * @return The number of bytes sent, or -1 on error.
     */
    ssize_t sendTo(const std::vector<uint8_t>& data, const std::string& address, uint16_t port);

    /**
     * @brief Set a callback function to be called when data is received.
     * @param callback The callback function.
     */
    void setMessageHandler(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback);

private:
    UDPSocket m_socket;
    bool m_running;
};

//=============================================================================
// UDP Client
//=============================================================================

/**
 * @class UDPClient
 * @brief A simple UDP client for sending and receiving datagrams.
 */
class UDPClient {
public:
    /**
     * @brief Default constructor.
     */
    UDPClient();

    /**
     * @brief Destructor.
     */
    ~UDPClient();

    /**
     * @brief Start the client.
     * @return True if successful, false otherwise.
     */
    bool start();

    /**
     * @brief Stop the client.
     */
    void stop();

    /**
     * @brief Check if the client is running.
     * @return True if the client is running, false otherwise.
     */
    bool isRunning() const;

    /**
     * @brief Send data to a specific server.
     * @param data The data to send.
     * @param address The server's address.
     * @param port The server's port.
     * @return The number of bytes sent, or -1 on error.
     */
    ssize_t send(const std::vector<uint8_t>& data, const std::string& address, uint16_t port);

    /**
     * @brief Set a callback function to be called when data is received.
     * @param callback The callback function.
     */
    void setMessageHandler(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback);

private:
    UDPSocket m_socket;
    bool m_running;
};

//=============================================================================
// HTTP Server (Forward declarations)
//=============================================================================

/**
 * @class HTTPServer
 * @brief A simple HTTP server.
 */
class HTTPServer {
public:
    /**
     * @brief Default constructor.
     */
    HTTPServer();

    /**
     * @brief Destructor.
     */
    ~HTTPServer();

    /**
     * @brief Start the server on the specified port.
     * @param port The port to listen on.
     * @return True if successful, false otherwise.
     */
    bool start(uint16_t port);

    /**
     * @brief Stop the server.
     */
    void stop();

    /**
     * @brief Check if the server is running.
     * @return True if the server is running, false otherwise.
     */
    bool isRunning() const;

    /**
     * @brief Set the web root directory.
     * @param webRoot The web root directory.
     */
    void setWebRoot(const std::string& webRoot);

    /**
     * @brief Set a callback function to be called when a request is received.
     * @param callback The callback function.
     */
    void setRequestHandler(std::function<void(const std::string&, const std::string&, std::string&)> callback);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

//=============================================================================
// HTTP Client (Forward declarations)
//=============================================================================

/**
 * @class HTTPClient
 * @brief A simple HTTP client.
 */
class HTTPClient {
public:
    /**
     * @brief Default constructor.
     */
    HTTPClient();

    /**
     * @brief Destructor.
     */
    ~HTTPClient();

    /**
     * @brief Send a GET request.
     * @param url The URL to send the request to.
     * @param response The response (output).
     * @return True if successful, false otherwise.
     */
    bool get(const std::string& url, std::string& response);

    /**
     * @brief Send a POST request.
     * @param url The URL to send the request to.
     * @param data The data to send.
     * @param response The response (output).
     * @return True if successful, false otherwise.
     */
    bool post(const std::string& url, const std::string& data, std::string& response);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

//=============================================================================
// DHT Socket Manager
//=============================================================================

/**
 * @class SocketManager
 * @brief Manages the UDP socket for a DHT node (Singleton)
 */
class SocketManager {
public:
    /**
     * @brief Gets the singleton instance of the socket manager
     * @param config The DHT configuration (only used if instance doesn't exist yet)
     * @return The singleton instance
     */
    static std::shared_ptr<SocketManager> getInstance(const dht_core::DHTConfig& config);

    /**
     * @brief Destructor
     */
    ~SocketManager();

    /**
     * @brief Delete copy constructor and assignment operator
     */
    SocketManager(const SocketManager&) = delete;
    SocketManager& operator=(const SocketManager&) = delete;
    SocketManager(SocketManager&&) = delete;
    SocketManager& operator=(SocketManager&&) = delete;

    /**
     * @brief Starts the socket manager
     * @param receiveCallback The callback to call when data is received
     * @return True if the socket manager was started successfully, false otherwise
     */
    bool start(std::function<void(const uint8_t*, size_t, const types::EndPoint&)> receiveCallback);

    /**
     * @brief Stops the socket manager
     */
    void stop();

    /**
     * @brief Checks if the socket manager is running
     * @return True if the socket manager is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Gets the port
     * @return The port
     */
    uint16_t getPort() const;

    /**
     * @brief Sends data to an endpoint
     * @param data The data to send
     * @param size The size of the data
     * @param endpoint The endpoint to send to
     * @return The number of bytes sent, or -1 on error
     */
    ssize_t sendTo(const void* data, size_t size, const types::EndPoint& endpoint);

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    explicit SocketManager(const dht_core::DHTConfig& config);

    // Static instance for singleton pattern
    static std::shared_ptr<SocketManager> s_instance;
    static std::recursive_mutex s_instanceMutex;

    dht_core::DHTConfig m_config;
    uint16_t m_port;
    std::atomic<bool> m_running;
    std::unique_ptr<UDPSocket> m_socket;
    std::recursive_mutex m_mutex;
};

//=============================================================================
// DHT Message Types
//=============================================================================

/**
 * @brief DHT message types
 */
enum class MessageType {
    QUERY,
    RESPONSE,
    ERROR
};

/**
 * @brief DHT query types
 */
enum class QueryType {
    PING,
    FIND_NODE,
    GET_PEERS,
    ANNOUNCE_PEER,
    UNKNOWN
};

/**
 * @brief Base class for DHT messages
 */
class Message {
public:
    /**
     * @brief Constructor
     * @param type The message type
     */
    explicit Message(MessageType type);

    /**
     * @brief Destructor
     */
    virtual ~Message() = default;

    /**
     * @brief Gets the message type
     * @return The message type
     */
    MessageType getType() const;

    /**
     * @brief Encodes the message to a bencode dictionary
     * @return The encoded message
     */
    virtual std::shared_ptr<common::BencodeValue> encode() const = 0;

    /**
     * @brief Decodes a bencode dictionary to a message
     * @param dict The bencode dictionary
     * @return True if the message was decoded successfully, false otherwise
     */
    virtual bool decode(const std::shared_ptr<common::BencodeValue>& dict) = 0;

protected:
    MessageType m_type;
};

/**
 * @brief DHT query message
 */
class QueryMessage : public Message {
public:
    /**
     * @brief Constructor
     * @param queryType The query type
     */
    explicit QueryMessage(QueryType queryType);

    /**
     * @brief Gets the query type
     * @return The query type
     */
    QueryType getQueryType() const;

    /**
     * @brief Sets the transaction ID
     * @param transactionID The transaction ID
     */
    void setTransactionID(const std::string& transactionID);

    /**
     * @brief Gets the transaction ID
     * @return The transaction ID
     */
    const std::string& getTransactionID() const;

    /**
     * @brief Sets the node ID
     * @param nodeID The node ID
     */
    void setNodeID(const types::NodeID& nodeID);

    /**
     * @brief Gets the node ID
     * @return The node ID
     */
    const types::NodeID& getNodeID() const;

    /**
     * @brief Encodes the message to a bencode dictionary
     * @return The encoded message
     */
    std::shared_ptr<common::BencodeValue> encode() const override;

    /**
     * @brief Decodes a bencode dictionary to a message
     * @param dict The bencode dictionary
     * @return True if the message was decoded successfully, false otherwise
     */
    bool decode(const std::shared_ptr<common::BencodeValue>& dict) override;

protected:
    QueryType m_queryType;
    std::string m_transactionID;
    types::NodeID m_nodeID;
};

/**
 * @brief DHT response message
 */
class ResponseMessage : public Message {
public:
    /**
     * @brief Constructor
     */
    ResponseMessage();

    /**
     * @brief Sets the transaction ID
     * @param transactionID The transaction ID
     */
    void setTransactionID(const std::string& transactionID);

    /**
     * @brief Gets the transaction ID
     * @return The transaction ID
     */
    const std::string& getTransactionID() const;

    /**
     * @brief Sets the node ID
     * @param nodeID The node ID
     */
    void setNodeID(const types::NodeID& nodeID);

    /**
     * @brief Gets the node ID
     * @return The node ID
     */
    const types::NodeID& getNodeID() const;

    /**
     * @brief Encodes the message to a bencode dictionary
     * @return The encoded message
     */
    std::shared_ptr<common::BencodeValue> encode() const override;

    /**
     * @brief Decodes a bencode dictionary to a message
     * @param dict The bencode dictionary
     * @return True if the message was decoded successfully, false otherwise
     */
    bool decode(const std::shared_ptr<common::BencodeValue>& dict) override;

protected:
    std::string m_transactionID;
    types::NodeID m_nodeID;
};

/**
 * @brief DHT error message
 */
class ErrorMessage : public Message {
public:
    /**
     * @brief Constructor
     */
    ErrorMessage();

    /**
     * @brief Sets the transaction ID
     * @param transactionID The transaction ID
     */
    void setTransactionID(const std::string& transactionID);

    /**
     * @brief Gets the transaction ID
     * @return The transaction ID
     */
    const std::string& getTransactionID() const;

    /**
     * @brief Sets the error code
     * @param code The error code
     */
    void setErrorCode(int code);

    /**
     * @brief Gets the error code
     * @return The error code
     */
    int getErrorCode() const;

    /**
     * @brief Sets the error message
     * @param message The error message
     */
    void setErrorMessage(const std::string& message);

    /**
     * @brief Gets the error message
     * @return The error message
     */
    const std::string& getErrorMessage() const;

    /**
     * @brief Encodes the message to a bencode dictionary
     * @return The encoded message
     */
    std::shared_ptr<common::BencodeValue> encode() const override;

    /**
     * @brief Decodes a bencode dictionary to a message
     * @param dict The bencode dictionary
     * @return True if the message was decoded successfully, false otherwise
     */
    bool decode(const std::shared_ptr<common::BencodeValue>& dict) override;

protected:
    std::string m_transactionID;
    int m_errorCode;
    std::string m_errorMessage;
};

//=============================================================================
// DHT Message Sender
//=============================================================================

/**
 * @brief Sends DHT messages (Singleton)
 */
class MessageSender {
public:
    /**
     * @brief Gets the singleton instance of the message sender
     * @param config The DHT configuration (only used if instance doesn't exist yet)
     * @param nodeID The node ID (only used if instance doesn't exist yet)
     * @param socketManager The socket manager (only used if instance doesn't exist yet)
     * @return The singleton instance
     */
    static std::shared_ptr<MessageSender> getInstance(
        const dht_core::DHTConfig& config,
        const types::NodeID& nodeID,
        std::shared_ptr<SocketManager> socketManager);

    /**
     * @brief Destructor
     */
    ~MessageSender();

    /**
     * @brief Delete copy constructor and assignment operator
     */
    MessageSender(const MessageSender&) = delete;
    MessageSender& operator=(const MessageSender&) = delete;
    MessageSender(MessageSender&&) = delete;
    MessageSender& operator=(MessageSender&&) = delete;

    /**
     * @brief Sends a query message
     * @param query The query message
     * @param endpoint The endpoint to send to
     * @return True if the message was sent successfully, false otherwise
     */
    bool sendQuery(std::shared_ptr<QueryMessage> query, const types::EndPoint& endpoint);

    /**
     * @brief Sends a response message
     * @param response The response message
     * @param endpoint The endpoint to send to
     * @return True if the message was sent successfully, false otherwise
     */
    bool sendResponse(std::shared_ptr<ResponseMessage> response, const types::EndPoint& endpoint);

    /**
     * @brief Sends an error message
     * @param error The error message
     * @param endpoint The endpoint to send to
     * @return True if the message was sent successfully, false otherwise
     */
    bool sendError(std::shared_ptr<ErrorMessage> error, const types::EndPoint& endpoint);

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    MessageSender(
        const dht_core::DHTConfig& config,
        const types::NodeID& nodeID,
        std::shared_ptr<SocketManager> socketManager);

    /**
     * @brief Sends a message
     * @param message The message
     * @param endpoint The endpoint to send to
     * @return True if the message was sent successfully, false otherwise
     */
    bool sendMessage(std::shared_ptr<Message> message, const types::EndPoint& endpoint);

    // Static instance for singleton pattern
    static std::shared_ptr<MessageSender> s_instance;
    static std::mutex s_instanceMutex;

    dht_core::DHTConfig m_config;
    types::NodeID m_nodeID;
    std::shared_ptr<SocketManager> m_socketManager;
    std::mutex m_mutex;
};

//=============================================================================
// DHT Message Handler
//=============================================================================

/**
 * @brief Handles DHT messages (Singleton)
 */
class MessageHandler {
public:
    /**
     * @brief Gets the singleton instance of the message handler
     * @param config The DHT configuration (only used if instance doesn't exist yet)
     * @param nodeID The node ID (only used if instance doesn't exist yet)
     * @param messageSender The message sender (only used if instance doesn't exist yet)
     * @param routingTable The routing table (only used if instance doesn't exist yet)
     * @return The singleton instance
     */
    static std::shared_ptr<MessageHandler> getInstance(
        const dht_core::DHTConfig& config,
        const types::NodeID& nodeID,
        std::shared_ptr<MessageSender> messageSender,
        std::shared_ptr<dht_core::RoutingTable> routingTable);

    /**
     * @brief Destructor
     */
    ~MessageHandler();

    /**
     * @brief Delete copy constructor and assignment operator
     */
    MessageHandler(const MessageHandler&) = delete;
    MessageHandler& operator=(const MessageHandler&) = delete;
    MessageHandler(MessageHandler&&) = delete;
    MessageHandler& operator=(MessageHandler&&) = delete;

    /**
     * @brief Handles a received message
     * @param data The message data
     * @param size The message size
     * @param endpoint The endpoint the message was received from
     */
    void handleMessage(const uint8_t* data, size_t size, const types::EndPoint& endpoint);

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    MessageHandler(
        const dht_core::DHTConfig& config,
        const types::NodeID& nodeID,
        std::shared_ptr<MessageSender> messageSender,
        std::shared_ptr<dht_core::RoutingTable> routingTable);

    /**
     * @brief Handles a query message
     * @param query The query message
     * @param endpoint The endpoint the message was received from
     */
    void handleQuery(std::shared_ptr<QueryMessage> query, const types::EndPoint& endpoint);

    /**
     * @brief Handles a response message
     * @param response The response message
     * @param endpoint The endpoint the message was received from
     */
    void handleResponse(std::shared_ptr<ResponseMessage> response, const types::EndPoint& endpoint);

    /**
     * @brief Handles an error message
     * @param error The error message
     * @param endpoint The endpoint the message was received from
     */
    void handleError(std::shared_ptr<ErrorMessage> error, const types::EndPoint& endpoint);

    // Static instance for singleton pattern
    static std::shared_ptr<MessageHandler> s_instance;
    static std::mutex s_instanceMutex;

    dht_core::DHTConfig m_config;
    types::NodeID m_nodeID;
    std::shared_ptr<MessageSender> m_messageSender;
    std::shared_ptr<dht_core::RoutingTable> m_routingTable;
    std::mutex m_mutex;
};

//=============================================================================
// DHT Transaction Manager
//=============================================================================

/**
 * @brief Callback for transaction responses
 */
using TransactionResponseCallback = std::function<void(std::shared_ptr<ResponseMessage>, const types::EndPoint&)>;

/**
 * @brief Callback for transaction errors
 */
using TransactionErrorCallback = std::function<void(std::shared_ptr<ErrorMessage>, const types::EndPoint&)>;

/**
 * @brief Callback for transaction timeouts
 */
using TransactionTimeoutCallback = std::function<void()>;

/**
 * @brief A transaction
 */
class Transaction {
public:
    /**
     * @brief Constructs a transaction
     * @param id The transaction ID
     * @param query The query message
     * @param endpoint The endpoint
     * @param responseCallback The response callback
     * @param errorCallback The error callback
     * @param timeoutCallback The timeout callback
     */
    Transaction(const std::string& id,
               std::shared_ptr<QueryMessage> query,
               const types::EndPoint& endpoint,
               TransactionResponseCallback responseCallback,
               TransactionErrorCallback errorCallback,
               TransactionTimeoutCallback timeoutCallback);

    /**
     * @brief Gets the transaction ID
     * @return The transaction ID
     */
    const std::string& getID() const;

    /**
     * @brief Gets the query message
     * @return The query message
     */
    std::shared_ptr<QueryMessage> getQuery() const;

    /**
     * @brief Gets the endpoint
     * @return The endpoint
     */
    const types::EndPoint& getEndpoint() const;

    /**
     * @brief Gets the timestamp
     * @return The timestamp
     */
    const std::chrono::steady_clock::time_point& getTimestamp() const;

    /**
     * @brief Handles a response
     * @param response The response message
     * @param endpoint The endpoint
     */
    void handleResponse(std::shared_ptr<ResponseMessage> response, const types::EndPoint& endpoint);

    /**
     * @brief Handles an error
     * @param error The error message
     * @param endpoint The endpoint
     */
    void handleError(std::shared_ptr<ErrorMessage> error, const types::EndPoint& endpoint);

    /**
     * @brief Handles a timeout
     */
    void handleTimeout();

private:
    std::string m_id;
    std::shared_ptr<QueryMessage> m_query;
    types::EndPoint m_endpoint;
    std::chrono::steady_clock::time_point m_timestamp;
    TransactionResponseCallback m_responseCallback;
    TransactionErrorCallback m_errorCallback;
    TransactionTimeoutCallback m_timeoutCallback;
};

/**
 * @brief Manages DHT transactions (Singleton)
 */
class TransactionManager {
public:
    /**
     * @brief Gets the singleton instance of the transaction manager
     * @param config The DHT configuration (only used if instance doesn't exist yet)
     * @param nodeID The node ID (only used if instance doesn't exist yet)
     * @return The singleton instance
     */
    static std::shared_ptr<TransactionManager> getInstance(
        const dht_core::DHTConfig& config,
        const types::NodeID& nodeID);

    /**
     * @brief Destructor
     */
    ~TransactionManager();

    /**
     * @brief Delete copy constructor and assignment operator
     */
    TransactionManager(const TransactionManager&) = delete;
    TransactionManager& operator=(const TransactionManager&) = delete;
    TransactionManager(TransactionManager&&) = delete;
    TransactionManager& operator=(TransactionManager&&) = delete;

    /**
     * @brief Creates a transaction
     * @param query The query message
     * @param endpoint The endpoint
     * @param responseCallback The response callback
     * @param errorCallback The error callback
     * @param timeoutCallback The timeout callback
     * @return The transaction ID
     */
    std::string createTransaction(std::shared_ptr<QueryMessage> query,
                                 const types::EndPoint& endpoint,
                                 TransactionResponseCallback responseCallback,
                                 TransactionErrorCallback errorCallback,
                                 TransactionTimeoutCallback timeoutCallback);

    /**
     * @brief Handles a response
     * @param response The response message
     * @param endpoint The endpoint
     * @return True if the response was handled, false otherwise
     */
    bool handleResponse(std::shared_ptr<ResponseMessage> response, const types::EndPoint& endpoint);

    /**
     * @brief Handles an error
     * @param error The error message
     * @param endpoint The endpoint
     * @return True if the error was handled, false otherwise
     */
    bool handleError(std::shared_ptr<ErrorMessage> error, const types::EndPoint& endpoint);

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    TransactionManager(const dht_core::DHTConfig& config, const types::NodeID& nodeID);

    /**
     * @brief Generates a random transaction ID
     * @return The transaction ID
     */
    std::string generateTransactionID();

    /**
     * @brief Checks for timed out transactions
     */
    void checkTimeouts();

    /**
     * @brief Checks for timed out transactions periodically
     */
    void checkTimeoutsPeriodically();

    // Static instance for singleton pattern
    static std::shared_ptr<TransactionManager> s_instance;
    static std::mutex s_instanceMutex;

    dht_core::DHTConfig m_config;
    types::NodeID m_nodeID;
    std::unordered_map<std::string, std::shared_ptr<Transaction>> m_transactions;
    std::atomic<bool> m_running;
    std::thread m_timeoutThread;
    std::mutex m_mutex;
    std::random_device m_rd;
    std::mt19937 m_gen;
};

} // namespace dht_hunter::utils::network
