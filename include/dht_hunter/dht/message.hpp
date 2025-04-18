#pragma once

#include "dht_hunter/bencode/bencode.hpp"
#include "dht_hunter/network/network_address.hpp"

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <array>
#include <random>

namespace dht_hunter::dht {

/**
 * @brief Node ID type (20 bytes)
 */
using NodeID = std::array<uint8_t, 20>;

/**
 * @brief Info hash type (20 bytes)
 */
using InfoHash = std::array<uint8_t, 20>;

/**
 * @brief Transaction ID type (variable length, usually 2-4 bytes)
 */
using TransactionID = std::vector<uint8_t>;

/**
 * @brief Compact node info (26 bytes: 20 bytes node ID + 6 bytes endpoint)
 */
struct CompactNodeInfo {
    NodeID id;
    network::EndPoint endpoint;
};

/**
 * @brief DHT message type
 */
enum class MessageType {
    Query,
    Response,
    Error
};

/**
 * @brief DHT query method
 */
enum class QueryMethod {
    Ping,
    FindNode,
    GetPeers,
    AnnouncePeer,
    Unknown
};

/**
 * @brief Converts a query method to a string
 * @param method The query method
 * @return The string representation of the query method
 */
std::string queryMethodToString(QueryMethod method);

/**
 * @brief Converts a string to a query method
 * @param method The string representation of the query method
 * @return The query method
 */
QueryMethod stringToQueryMethod(const std::string& method);

/**
 * @brief Generates a random node ID
 * @return A random node ID
 */
NodeID generateRandomNodeID();

/**
 * @brief Generates a random transaction ID
 * @param length The length of the transaction ID (default: 2)
 * @return A random transaction ID
 */
TransactionID generateTransactionID(size_t length = 2);

/**
 * @brief Converts a node ID to a string
 * @param id The node ID
 * @return The string representation of the node ID
 */
std::string nodeIDToString(const NodeID& id);

/**
 * @brief Converts a string to a node ID
 * @param str The string representation of the node ID
 * @return The node ID, or std::nullopt if the string is invalid
 */
std::optional<NodeID> stringToNodeID(const std::string& str);

/**
 * @brief Converts a transaction ID to a string
 * @param id The transaction ID
 * @return The string representation of the transaction ID
 */
std::string transactionIDToString(const TransactionID& id);

/**
 * @brief Converts a string to a transaction ID
 * @param str The string representation of the transaction ID
 * @return The transaction ID
 */
TransactionID stringToTransactionID(const std::string& str);

/**
 * @brief Parses compact node info from a string
 * @param data The compact node info string
 * @return A vector of compact node info
 */
std::vector<CompactNodeInfo> parseCompactNodeInfo(const std::string& data);

/**
 * @brief Encodes compact node info to a string
 * @param nodes The compact node info
 * @return The encoded string
 */
std::string encodeCompactNodeInfo(const std::vector<CompactNodeInfo>& nodes);

/**
 * @class Message
 * @brief Base class for DHT messages
 */
class Message {
public:
    /**
     * @brief Constructs a message
     * @param type The message type
     * @param transactionID The transaction ID
     */
    Message(MessageType type, const TransactionID& transactionID);
    
    /**
     * @brief Virtual destructor
     */
    virtual ~Message() = default;
    
    /**
     * @brief Gets the message type
     * @return The message type
     */
    MessageType getType() const;
    
    /**
     * @brief Gets the transaction ID
     * @return The transaction ID
     */
    const TransactionID& getTransactionID() const;
    
    /**
     * @brief Encodes the message to a bencode value
     * @return The encoded message
     */
    virtual std::shared_ptr<bencode::BencodeValue> encode() const = 0;
    
    /**
     * @brief Encodes the message to a string
     * @return The encoded message
     */
    std::string encodeToString() const;
    
protected:
    MessageType m_type;
    TransactionID m_transactionID;
};

/**
 * @class QueryMessage
 * @brief DHT query message
 */
class QueryMessage : public Message {
public:
    /**
     * @brief Constructs a query message
     * @param method The query method
     * @param transactionID The transaction ID
     * @param nodeID The sender's node ID
     */
    QueryMessage(QueryMethod method, const TransactionID& transactionID, const NodeID& nodeID);
    
    /**
     * @brief Gets the query method
     * @return The query method
     */
    QueryMethod getMethod() const;
    
    /**
     * @brief Gets the sender's node ID
     * @return The sender's node ID
     */
    const NodeID& getNodeID() const;
    
    /**
     * @brief Encodes the message to a bencode value
     * @return The encoded message
     */
    std::shared_ptr<bencode::BencodeValue> encode() const override;
    
    /**
     * @brief Creates a query message from a bencode value
     * @param value The bencode value
     * @return The query message, or nullptr if the value is invalid
     */
    static std::shared_ptr<QueryMessage> decode(const bencode::BencodeValue& value);
    
protected:
    QueryMethod m_method;
    NodeID m_nodeID;
    
    /**
     * @brief Encodes the arguments to a bencode value
     * @return The encoded arguments
     */
    virtual std::shared_ptr<bencode::BencodeValue> encodeArguments() const;
};

/**
 * @class PingQuery
 * @brief DHT ping query
 */
class PingQuery : public QueryMessage {
public:
    /**
     * @brief Constructs a ping query
     * @param transactionID The transaction ID
     * @param nodeID The sender's node ID
     */
    PingQuery(const TransactionID& transactionID, const NodeID& nodeID);
    
    /**
     * @brief Creates a ping query from a bencode value
     * @param value The bencode value
     * @return The ping query, or nullptr if the value is invalid
     */
    static std::shared_ptr<PingQuery> decode(const bencode::BencodeValue& value);
};

/**
 * @class FindNodeQuery
 * @brief DHT find_node query
 */
class FindNodeQuery : public QueryMessage {
public:
    /**
     * @brief Constructs a find_node query
     * @param transactionID The transaction ID
     * @param nodeID The sender's node ID
     * @param target The target node ID
     */
    FindNodeQuery(const TransactionID& transactionID, const NodeID& nodeID, const NodeID& target);
    
    /**
     * @brief Gets the target node ID
     * @return The target node ID
     */
    const NodeID& getTarget() const;
    
    /**
     * @brief Encodes the arguments to a bencode value
     * @return The encoded arguments
     */
    std::shared_ptr<bencode::BencodeValue> encodeArguments() const override;
    
    /**
     * @brief Creates a find_node query from a bencode value
     * @param value The bencode value
     * @return The find_node query, or nullptr if the value is invalid
     */
    static std::shared_ptr<FindNodeQuery> decode(const bencode::BencodeValue& value);
    
private:
    NodeID m_target;
};

/**
 * @class GetPeersQuery
 * @brief DHT get_peers query
 */
class GetPeersQuery : public QueryMessage {
public:
    /**
     * @brief Constructs a get_peers query
     * @param transactionID The transaction ID
     * @param nodeID The sender's node ID
     * @param infoHash The info hash
     */
    GetPeersQuery(const TransactionID& transactionID, const NodeID& nodeID, const InfoHash& infoHash);
    
    /**
     * @brief Gets the info hash
     * @return The info hash
     */
    const InfoHash& getInfoHash() const;
    
    /**
     * @brief Encodes the arguments to a bencode value
     * @return The encoded arguments
     */
    std::shared_ptr<bencode::BencodeValue> encodeArguments() const override;
    
    /**
     * @brief Creates a get_peers query from a bencode value
     * @param value The bencode value
     * @return The get_peers query, or nullptr if the value is invalid
     */
    static std::shared_ptr<GetPeersQuery> decode(const bencode::BencodeValue& value);
    
private:
    InfoHash m_infoHash;
};

/**
 * @class AnnouncePeerQuery
 * @brief DHT announce_peer query
 */
class AnnouncePeerQuery : public QueryMessage {
public:
    /**
     * @brief Constructs an announce_peer query
     * @param transactionID The transaction ID
     * @param nodeID The sender's node ID
     * @param infoHash The info hash
     * @param port The port
     * @param token The token received from a previous get_peers query
     * @param impliedPort Whether to use the port from the packet (BEP 5)
     */
    AnnouncePeerQuery(const TransactionID& transactionID, const NodeID& nodeID, const InfoHash& infoHash,
                     uint16_t port, const std::string& token, bool impliedPort = false);
    
    /**
     * @brief Gets the info hash
     * @return The info hash
     */
    const InfoHash& getInfoHash() const;
    
    /**
     * @brief Gets the port
     * @return The port
     */
    uint16_t getPort() const;
    
    /**
     * @brief Gets the token
     * @return The token
     */
    const std::string& getToken() const;
    
    /**
     * @brief Checks if implied_port is set
     * @return True if implied_port is set, false otherwise
     */
    bool isImpliedPort() const;
    
    /**
     * @brief Encodes the arguments to a bencode value
     * @return The encoded arguments
     */
    std::shared_ptr<bencode::BencodeValue> encodeArguments() const override;
    
    /**
     * @brief Creates an announce_peer query from a bencode value
     * @param value The bencode value
     * @return The announce_peer query, or nullptr if the value is invalid
     */
    static std::shared_ptr<AnnouncePeerQuery> decode(const bencode::BencodeValue& value);
    
private:
    InfoHash m_infoHash;
    uint16_t m_port;
    std::string m_token;
    bool m_impliedPort;
};

/**
 * @class ResponseMessage
 * @brief DHT response message
 */
class ResponseMessage : public Message {
public:
    /**
     * @brief Constructs a response message
     * @param transactionID The transaction ID
     * @param nodeID The sender's node ID
     */
    ResponseMessage(const TransactionID& transactionID, const NodeID& nodeID);
    
    /**
     * @brief Gets the sender's node ID
     * @return The sender's node ID
     */
    const NodeID& getNodeID() const;
    
    /**
     * @brief Encodes the message to a bencode value
     * @return The encoded message
     */
    std::shared_ptr<bencode::BencodeValue> encode() const override;
    
    /**
     * @brief Creates a response message from a bencode value
     * @param value The bencode value
     * @return The response message, or nullptr if the value is invalid
     */
    static std::shared_ptr<ResponseMessage> decode(const bencode::BencodeValue& value);
    
protected:
    NodeID m_nodeID;
    
    /**
     * @brief Encodes the response values to a bencode value
     * @return The encoded response values
     */
    virtual std::shared_ptr<bencode::BencodeValue> encodeValues() const;
};

/**
 * @class PingResponse
 * @brief DHT ping response
 */
class PingResponse : public ResponseMessage {
public:
    /**
     * @brief Constructs a ping response
     * @param transactionID The transaction ID
     * @param nodeID The sender's node ID
     */
    PingResponse(const TransactionID& transactionID, const NodeID& nodeID);
    
    /**
     * @brief Creates a ping response from a bencode value
     * @param value The bencode value
     * @return The ping response, or nullptr if the value is invalid
     */
    static std::shared_ptr<PingResponse> decode(const bencode::BencodeValue& value);
};

/**
 * @class FindNodeResponse
 * @brief DHT find_node response
 */
class FindNodeResponse : public ResponseMessage {
public:
    /**
     * @brief Constructs a find_node response
     * @param transactionID The transaction ID
     * @param nodeID The sender's node ID
     * @param nodes The compact node info
     */
    FindNodeResponse(const TransactionID& transactionID, const NodeID& nodeID, const std::vector<CompactNodeInfo>& nodes);
    
    /**
     * @brief Gets the compact node info
     * @return The compact node info
     */
    const std::vector<CompactNodeInfo>& getNodes() const;
    
    /**
     * @brief Encodes the response values to a bencode value
     * @return The encoded response values
     */
    std::shared_ptr<bencode::BencodeValue> encodeValues() const override;
    
    /**
     * @brief Creates a find_node response from a bencode value
     * @param value The bencode value
     * @return The find_node response, or nullptr if the value is invalid
     */
    static std::shared_ptr<FindNodeResponse> decode(const bencode::BencodeValue& value);
    
private:
    std::vector<CompactNodeInfo> m_nodes;
};

/**
 * @class GetPeersResponse
 * @brief DHT get_peers response
 */
class GetPeersResponse : public ResponseMessage {
public:
    /**
     * @brief Constructs a get_peers response with nodes
     * @param transactionID The transaction ID
     * @param nodeID The sender's node ID
     * @param nodes The compact node info
     * @param token The token
     */
    GetPeersResponse(const TransactionID& transactionID, const NodeID& nodeID, 
                    const std::vector<CompactNodeInfo>& nodes, const std::string& token);
    
    /**
     * @brief Constructs a get_peers response with peers
     * @param transactionID The transaction ID
     * @param nodeID The sender's node ID
     * @param peers The compact peer info
     * @param token The token
     */
    GetPeersResponse(const TransactionID& transactionID, const NodeID& nodeID,
                    const std::vector<network::EndPoint>& peers, const std::string& token);
    
    /**
     * @brief Gets the compact node info
     * @return The compact node info
     */
    const std::vector<CompactNodeInfo>& getNodes() const;
    
    /**
     * @brief Gets the compact peer info
     * @return The compact peer info
     */
    const std::vector<network::EndPoint>& getPeers() const;
    
    /**
     * @brief Gets the token
     * @return The token
     */
    const std::string& getToken() const;
    
    /**
     * @brief Checks if the response has nodes
     * @return True if the response has nodes, false otherwise
     */
    bool hasNodes() const;
    
    /**
     * @brief Checks if the response has peers
     * @return True if the response has peers, false otherwise
     */
    bool hasPeers() const;
    
    /**
     * @brief Encodes the response values to a bencode value
     * @return The encoded response values
     */
    std::shared_ptr<bencode::BencodeValue> encodeValues() const override;
    
    /**
     * @brief Creates a get_peers response from a bencode value
     * @param value The bencode value
     * @return The get_peers response, or nullptr if the value is invalid
     */
    static std::shared_ptr<GetPeersResponse> decode(const bencode::BencodeValue& value);
    
private:
    std::vector<CompactNodeInfo> m_nodes;
    std::vector<network::EndPoint> m_peers;
    std::string m_token;
    bool m_hasNodes;
    bool m_hasPeers;
};

/**
 * @class AnnouncePeerResponse
 * @brief DHT announce_peer response
 */
class AnnouncePeerResponse : public ResponseMessage {
public:
    /**
     * @brief Constructs an announce_peer response
     * @param transactionID The transaction ID
     * @param nodeID The sender's node ID
     */
    AnnouncePeerResponse(const TransactionID& transactionID, const NodeID& nodeID);
    
    /**
     * @brief Creates an announce_peer response from a bencode value
     * @param value The bencode value
     * @return The announce_peer response, or nullptr if the value is invalid
     */
    static std::shared_ptr<AnnouncePeerResponse> decode(const bencode::BencodeValue& value);
};

/**
 * @class ErrorMessage
 * @brief DHT error message
 */
class ErrorMessage : public Message {
public:
    /**
     * @brief Constructs an error message
     * @param transactionID The transaction ID
     * @param code The error code
     * @param message The error message
     */
    ErrorMessage(const TransactionID& transactionID, int code, const std::string& message);
    
    /**
     * @brief Gets the error code
     * @return The error code
     */
    int getCode() const;
    
    /**
     * @brief Gets the error message
     * @return The error message
     */
    const std::string& getMessage() const;
    
    /**
     * @brief Encodes the message to a bencode value
     * @return The encoded message
     */
    std::shared_ptr<bencode::BencodeValue> encode() const override;
    
    /**
     * @brief Creates an error message from a bencode value
     * @param value The bencode value
     * @return The error message, or nullptr if the value is invalid
     */
    static std::shared_ptr<ErrorMessage> decode(const bencode::BencodeValue& value);
    
private:
    int m_code;
    std::string m_message;
};

/**
 * @brief Decodes a DHT message from a bencode value
 * @param value The bencode value
 * @return The decoded message, or nullptr if the value is invalid
 */
std::shared_ptr<Message> decodeMessage(const bencode::BencodeValue& value);

/**
 * @brief Decodes a DHT message from a string
 * @param data The string
 * @return The decoded message, or nullptr if the string is invalid
 */
std::shared_ptr<Message> decodeMessage(const std::string& data);

} // namespace dht_hunter::dht
