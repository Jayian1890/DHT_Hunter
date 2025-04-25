#pragma once

#include "dht_hunter/dht/network/dht_message.hpp"
#include <string>
#include <vector>

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
 * @brief Converts a query method to a string
 * @param method The query method
 * @return The string representation of the query method
 */
std::string queryMethodToString(QueryMethod method);

/**
 * @brief Base class for all DHT query messages
 */
class QueryMessage : public DHTMessage {
public:
    /**
     * @brief Constructs a query message
     * @param transactionID The transaction ID
     * @param method The query method
     * @param nodeID The node ID of the sender
     */
    QueryMessage(const std::string& transactionID, QueryMethod method, const NodeID& nodeID);

    /**
     * @brief Gets the message type
     * @return The message type (always Query)
     */
    MessageType getType() const override;

    /**
     * @brief Gets the query method
     * @return The query method
     */
    QueryMethod getMethod() const;

    /**
     * @brief Gets the node ID of the sender
     * @return The node ID of the sender
     */
    const NodeID& getNodeIDRef() const;

    /**
     * @brief Gets the node ID of the sender as an optional
     * @return The node ID of the sender
     */
    std::optional<NodeID> getNodeID() const override;

    /**
     * @brief Sets the node ID of the sender
     * @param nodeID The node ID of the sender
     */
    void setNodeID(const NodeID& nodeID);

    /**
     * @brief Encodes the query message to a bencode dictionary
     * @return The encoded query message
     */
    bencode::BencodeDict encode() const override;

    /**
     * @brief Decodes a query message from a bencode dictionary
     * @param dict The bencode dictionary
     * @return The decoded query message, or nullptr if decoding failed
     */
    static std::shared_ptr<QueryMessage> decode(const bencode::BencodeDict& dict);

protected:
    /**
     * @brief Encodes the query-specific arguments to a bencode dictionary
     * @param dict The bencode dictionary to add the arguments to
     */
    virtual void encodeArguments(bencode::BencodeDict& dict) const = 0;

    /**
     * @brief Decodes the query-specific arguments from a bencode dictionary
     * @param dict The bencode dictionary to get the arguments from
     * @return True if decoding was successful, false otherwise
     */
    virtual bool decodeArguments(const bencode::BencodeDict& dict) = 0;

    QueryMethod m_method;
    NodeID m_nodeID;
};

/**
 * @brief DHT ping query message
 */
class PingQuery : public QueryMessage {
public:
    /**
     * @brief Constructs a ping query message
     * @param transactionID The transaction ID (optional)
     * @param nodeID The node ID of the sender (optional)
     */
    explicit PingQuery(const std::string& transactionID = "", const NodeID& nodeID = NodeID());

protected:
    /**
     * @brief Encodes the ping query arguments to a bencode dictionary
     * @param dict The bencode dictionary to add the arguments to
     */
    void encodeArguments(bencode::BencodeDict& dict) const override;

    /**
     * @brief Decodes the ping query arguments from a bencode dictionary
     * @param dict The bencode dictionary to get the arguments from
     * @return True if decoding was successful, false otherwise
     */
    bool decodeArguments(const bencode::BencodeDict& dict) override;
};

/**
 * @brief DHT find_node query message
 */
class FindNodeQuery : public QueryMessage {
public:
    /**
     * @brief Constructs a find_node query message
     * @param transactionID The transaction ID (optional)
     * @param nodeID The node ID of the sender (optional)
     * @param targetID The target node ID to find (optional)
     */
    FindNodeQuery(const std::string& transactionID = "", const NodeID& nodeID = NodeID(), const NodeID& targetID = NodeID());

    /**
     * @brief Constructs a find_node query message with just a target ID
     * @param targetID The target ID to find
     */
    explicit FindNodeQuery(const NodeID& targetID);

    /**
     * @brief Gets the target node ID
     * @return The target node ID
     */
    const NodeID& getTargetID() const;

    /**
     * @brief Sets the target node ID
     * @param targetID The target node ID
     */
    void setTargetID(const NodeID& targetID);

protected:
    /**
     * @brief Encodes the find_node query arguments to a bencode dictionary
     * @param dict The bencode dictionary to add the arguments to
     */
    void encodeArguments(bencode::BencodeDict& dict) const override;

    /**
     * @brief Decodes the find_node query arguments from a bencode dictionary
     * @param dict The bencode dictionary to get the arguments from
     * @return True if decoding was successful, false otherwise
     */
    bool decodeArguments(const bencode::BencodeDict& dict) override;

private:
    NodeID m_targetID;
};

/**
 * @brief DHT get_peers query message
 */
class GetPeersQuery : public QueryMessage {
public:
    /**
     * @brief Constructs a get_peers query message
     * @param transactionID The transaction ID (optional)
     * @param nodeID The node ID of the sender (optional)
     * @param infoHash The info hash to get peers for (optional)
     */
    GetPeersQuery(const std::string& transactionID = "", const NodeID& nodeID = NodeID(), const InfoHash& infoHash = InfoHash());

    /**
     * @brief Constructs a get_peers query message with just an info hash
     * @param infoHash The info hash to get peers for
     */
    explicit GetPeersQuery(const InfoHash& infoHash);

    /**
     * @brief Gets the info hash
     * @return The info hash
     */
    const InfoHash& getInfoHash() const;

    /**
     * @brief Sets the info hash
     * @param infoHash The info hash
     */
    void setInfoHash(const InfoHash& infoHash);

protected:
    /**
     * @brief Encodes the get_peers query arguments to a bencode dictionary
     * @param dict The bencode dictionary to add the arguments to
     */
    void encodeArguments(bencode::BencodeDict& dict) const override;

    /**
     * @brief Decodes the get_peers query arguments from a bencode dictionary
     * @param dict The bencode dictionary to get the arguments from
     * @return True if decoding was successful, false otherwise
     */
    bool decodeArguments(const bencode::BencodeDict& dict) override;

private:
    InfoHash m_infoHash;
};

/**
 * @brief DHT announce_peer query message
 */
class AnnouncePeerQuery : public QueryMessage {
public:
    /**
     * @brief Constructs an announce_peer query message
     * @param transactionID The transaction ID (optional)
     * @param nodeID The node ID of the sender (optional)
     * @param infoHash The info hash to announce (optional)
     * @param port The port to announce (optional)
     * @param implied_port Whether to use the sender's port instead of the specified port (optional)
     */
    AnnouncePeerQuery(const std::string& transactionID = "", const NodeID& nodeID = NodeID(), const InfoHash& infoHash = InfoHash(), uint16_t port = 0, bool implied_port = false);

    /**
     * @brief Constructs an announce_peer query message with just an info hash and port
     * @param infoHash The info hash to announce
     * @param port The port to announce
     */
    AnnouncePeerQuery(const InfoHash& infoHash, uint16_t port);

    /**
     * @brief Gets the info hash
     * @return The info hash
     */
    const InfoHash& getInfoHash() const;

    /**
     * @brief Sets the info hash
     * @param infoHash The info hash
     */
    void setInfoHash(const InfoHash& infoHash);

    /**
     * @brief Gets the port
     * @return The port
     */
    uint16_t getPort() const;

    /**
     * @brief Sets the port
     * @param port The port
     */
    void setPort(uint16_t port);

    /**
     * @brief Gets whether to use the sender's port
     * @return True if the sender's port should be used, false otherwise
     */
    bool getImpliedPort() const;

    /**
     * @brief Sets whether to use the sender's port
     * @param implied_port True if the sender's port should be used, false otherwise
     */
    void setImpliedPort(bool implied_port);

    /**
     * @brief Gets the token
     * @return The token
     */
    const std::string& getToken() const;

    /**
     * @brief Sets the token
     * @param token The token
     */
    void setToken(const std::string& token);

protected:
    /**
     * @brief Encodes the announce_peer query arguments to a bencode dictionary
     * @param dict The bencode dictionary to add the arguments to
     */
    void encodeArguments(bencode::BencodeDict& dict) const override;

    /**
     * @brief Decodes the announce_peer query arguments from a bencode dictionary
     * @param dict The bencode dictionary to get the arguments from
     * @return True if decoding was successful, false otherwise
     */
    bool decodeArguments(const bencode::BencodeDict& dict) override;

private:
    InfoHash m_infoHash;
    uint16_t m_port;
    bool m_implied_port;
    std::string m_token;
};

} // namespace dht_hunter::dht
