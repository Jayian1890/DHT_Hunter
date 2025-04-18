#pragma once

#include "dht_hunter/dht/message.hpp"

namespace dht_hunter::dht {

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

} // namespace dht_hunter::dht
