#pragma once

#include "dht_hunter/dht/network/message.hpp"
#include <string>
#include <memory>
#include <optional>
#include <vector>
#include "dht_hunter/bencode/bencode.hpp"

namespace dht_hunter::dht {

/**
 * @brief Enum for the different types of DHT queries
 */
enum class QueryMethod {
    Ping,
    FindNode,
    GetPeers,
    AnnouncePeer
};

/**
 * @brief Base class for all DHT query messages
 */
class QueryMessage : public Message {
public:
    /**
     * @brief Constructs a query message
     * @param transactionID The transaction ID
     * @param nodeID The node ID
     * @param method The query method
     */
    QueryMessage(const std::string& transactionID, const NodeID& nodeID, QueryMethod method);

    /**
     * @brief Gets the message type
     * @return The message type
     */
    MessageType getType() const override;

    /**
     * @brief Gets the query method
     * @return The query method
     */
    QueryMethod getMethod() const;

    /**
     * @brief Encodes the message to a byte vector
     * @return The encoded message
     */
    std::vector<uint8_t> encode() const override;

    /**
     * @brief Decodes a query message from a bencode value
     * @param value The bencode value
     * @return The decoded query message, or nullptr if decoding failed
     */
    static std::shared_ptr<QueryMessage> decode(const dht_hunter::bencode::BencodeValue& value);

protected:
    /**
     * @brief Gets the method name
     * @return The method name
     */
    virtual std::string getMethodName() const = 0;

    /**
     * @brief Gets the arguments for the query
     * @return The arguments
     */
    virtual std::shared_ptr<dht_hunter::bencode::BencodeValue> getArguments() const = 0;

    QueryMethod m_method;
};

/**
 * @brief DHT ping query
 */
class PingQuery : public QueryMessage {
public:
    /**
     * @brief Constructs a ping query
     * @param transactionID The transaction ID
     * @param nodeID The node ID
     */
    PingQuery(const std::string& transactionID, const NodeID& nodeID);

    /**
     * @brief Creates a ping query from a bencode dictionary
     * @param transactionID The transaction ID
     * @param nodeID The node ID
     * @param arguments The arguments
     * @return The ping query
     */
    static std::shared_ptr<PingQuery> create(const std::string& transactionID, const NodeID& nodeID, const dht_hunter::bencode::BencodeValue& arguments);

protected:
    /**
     * @brief Gets the method name
     * @return The method name
     */
    std::string getMethodName() const override;

    /**
     * @brief Gets the arguments for the query
     * @return The arguments
     */
    std::shared_ptr<dht_hunter::bencode::BencodeValue> getArguments() const override;
};

/**
 * @brief DHT find_node query
 */
class FindNodeQuery : public QueryMessage {
public:
    /**
     * @brief Constructs a find_node query
     * @param transactionID The transaction ID
     * @param nodeID The node ID
     * @param targetID The target node ID
     */
    FindNodeQuery(const std::string& transactionID, const NodeID& nodeID, const NodeID& targetID);

    /**
     * @brief Gets the target node ID
     * @return The target node ID
     */
    const NodeID& getTargetID() const;

    /**
     * @brief Creates a find_node query from a bencode dictionary
     * @param transactionID The transaction ID
     * @param nodeID The node ID
     * @param arguments The arguments
     * @return The find_node query
     */
    static std::shared_ptr<FindNodeQuery> create(const std::string& transactionID, const NodeID& nodeID, const dht_hunter::bencode::BencodeValue& arguments);

protected:
    /**
     * @brief Gets the method name
     * @return The method name
     */
    std::string getMethodName() const override;

    /**
     * @brief Gets the arguments for the query
     * @return The arguments
     */
    std::shared_ptr<dht_hunter::bencode::BencodeValue> getArguments() const override;

private:
    NodeID m_targetID;
};

/**
 * @brief DHT get_peers query
 */
class GetPeersQuery : public QueryMessage {
public:
    /**
     * @brief Constructs a get_peers query
     * @param transactionID The transaction ID
     * @param nodeID The node ID
     * @param infoHash The info hash
     */
    GetPeersQuery(const std::string& transactionID, const NodeID& nodeID, const InfoHash& infoHash);

    /**
     * @brief Gets the info hash
     * @return The info hash
     */
    const InfoHash& getInfoHash() const;

    /**
     * @brief Creates a get_peers query from a bencode dictionary
     * @param transactionID The transaction ID
     * @param nodeID The node ID
     * @param arguments The arguments
     * @return The get_peers query
     */
    static std::shared_ptr<GetPeersQuery> create(const std::string& transactionID, const NodeID& nodeID, const dht_hunter::bencode::BencodeValue& arguments);

protected:
    /**
     * @brief Gets the method name
     * @return The method name
     */
    std::string getMethodName() const override;

    /**
     * @brief Gets the arguments for the query
     * @return The arguments
     */
    std::shared_ptr<dht_hunter::bencode::BencodeValue> getArguments() const override;

private:
    InfoHash m_infoHash;
};

/**
 * @brief DHT announce_peer query
 */
class AnnouncePeerQuery : public QueryMessage {
public:
    /**
     * @brief Constructs an announce_peer query
     * @param transactionID The transaction ID
     * @param nodeID The node ID
     * @param infoHash The info hash
     * @param port The port
     * @param token The token
     * @param impliedPort Whether to use the implied port
     */
    AnnouncePeerQuery(const std::string& transactionID, const NodeID& nodeID, const InfoHash& infoHash, uint16_t port, const std::string& token, bool impliedPort = false);

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
     * @brief Checks if the implied port flag is set
     * @return True if the implied port flag is set, false otherwise
     */
    bool isImpliedPort() const;

    /**
     * @brief Creates an announce_peer query from a bencode dictionary
     * @param transactionID The transaction ID
     * @param nodeID The node ID
     * @param arguments The arguments
     * @return The announce_peer query
     */
    static std::shared_ptr<AnnouncePeerQuery> create(const std::string& transactionID, const NodeID& nodeID, const dht_hunter::bencode::BencodeValue& arguments);

protected:
    /**
     * @brief Gets the method name
     * @return The method name
     */
    std::string getMethodName() const override;

    /**
     * @brief Gets the arguments for the query
     * @return The arguments
     */
    std::shared_ptr<dht_hunter::bencode::BencodeValue> getArguments() const override;

private:
    InfoHash m_infoHash;
    uint16_t m_port;
    std::string m_token;
    bool m_impliedPort;
};

} // namespace dht_hunter::dht
