#pragma once

#include "dht_hunter/dht/network/dht_message.hpp"
#include "dht_hunter/dht/network/dht_query_message.hpp"
#include <string>
#include <vector>

namespace dht_hunter::dht {

/**
 * @brief Base class for all DHT response messages
 */
class ResponseMessage : public DHTMessage {
public:
    /**
     * @brief Constructs a response message
     * @param transactionID The transaction ID
     * @param nodeID The node ID of the sender
     */
    ResponseMessage(const std::string& transactionID, const NodeID& nodeID);

    /**
     * @brief Constructs a response message with an empty node ID
     * @param transactionID The transaction ID
     */
    explicit ResponseMessage(const std::string& transactionID);

    /**
     * @brief Virtual destructor
     */
    virtual ~ResponseMessage() = default;

    /**
     * @brief Gets the message type
     * @return The message type (always Response)
     */
    MessageType getType() const override;

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
     * @brief Sets the token for this response
     * @param token The token to set
     */
    virtual void setToken(const std::string& /*token*/) {}

    /**
     * @brief Gets the token for this response
     * @return The token, if available
     */
    virtual std::optional<std::string> getToken() const { return std::nullopt; }

    /**
     * @brief Gets the nodes in the response
     * @return The nodes, empty vector by default
     */
    virtual const std::vector<std::shared_ptr<Node>>& getNodes() const {
        static const std::vector<std::shared_ptr<Node>> emptyNodes;
        return emptyNodes;
    }

    /**
     * @brief Encodes the response message to a bencode dictionary
     * @return The encoded response message
     */
    bencode::BencodeDict encode() const override;

    /**
     * @brief Decodes a response message from a bencode dictionary
     * @param dict The bencode dictionary
     * @param queryMethod The method of the query that this is a response to
     * @return The decoded response message, or nullptr if decoding failed
     */
    static std::shared_ptr<ResponseMessage> decode(const bencode::BencodeDict& dict, QueryMethod queryMethod);

protected:
    /**
     * @brief Encodes the response-specific values to a bencode dictionary
     * @param dict The bencode dictionary to add the values to
     */
    virtual void encodeValues(bencode::BencodeDict& dict) const = 0;

    /**
     * @brief Decodes the response-specific values from a bencode dictionary
     * @param dict The bencode dictionary to get the values from
     * @return True if decoding was successful, false otherwise
     */
    virtual bool decodeValues(const bencode::BencodeDict& dict) = 0;

    NodeID m_nodeID;
};

/**
 * @brief DHT ping response message
 */
class PingResponse : public ResponseMessage {
public:
    /**
     * @brief Constructs a ping response message
     * @param transactionID The transaction ID
     * @param nodeID The node ID of the sender
     */
    PingResponse(const std::string& transactionID = "", const NodeID& nodeID = NodeID());

protected:
    /**
     * @brief Encodes the ping response values to a bencode dictionary
     * @param dict The bencode dictionary to add the values to
     */
    void encodeValues(bencode::BencodeDict& dict) const override;

    /**
     * @brief Decodes the ping response values from a bencode dictionary
     * @param dict The bencode dictionary to get the values from
     * @return True if decoding was successful, false otherwise
     */
    bool decodeValues(const bencode::BencodeDict& dict) override;
};

/**
 * @brief DHT find_node response message
 */
class FindNodeResponse : public ResponseMessage {
public:
    /**
     * @brief Constructs a find_node response message
     * @param transactionID The transaction ID
     * @param nodeID The node ID of the sender
     */
    FindNodeResponse(const std::string& transactionID = "", const NodeID& nodeID = NodeID());

    /**
     * @brief Gets the nodes
     * @return The nodes
     */
    const std::vector<std::shared_ptr<Node>>& getNodes() const override;

    /**
     * @brief Adds a node
     * @param node The node to add
     */
    void addNode(std::shared_ptr<Node> node);

    /**
     * @brief Sets the nodes
     * @param nodes The nodes
     */
    void setNodes(const std::vector<std::shared_ptr<Node>>& nodes);

protected:
    /**
     * @brief Encodes the find_node response values to a bencode dictionary
     * @param dict The bencode dictionary to add the values to
     */
    void encodeValues(bencode::BencodeDict& dict) const override;

    /**
     * @brief Decodes the find_node response values from a bencode dictionary
     * @param dict The bencode dictionary to get the values from
     * @return True if decoding was successful, false otherwise
     */
    bool decodeValues(const bencode::BencodeDict& dict) override;

private:
    std::vector<std::shared_ptr<Node>> m_nodes;
};

/**
 * @brief DHT get_peers response message
 */
class GetPeersResponse : public ResponseMessage {
public:
    /**
     * @brief Constructs a get_peers response message
     * @param transactionID The transaction ID
     * @param nodeID The node ID of the sender
     */
    GetPeersResponse(const std::string& transactionID = "", const NodeID& nodeID = NodeID());

    /**
     * @brief Gets the token
     * @return The token
     */
    std::optional<std::string> getToken() const override;

    /**
     * @brief Sets the token
     * @param token The token
     */
    void setToken(const std::string& token) override;

    /**
     * @brief Gets the nodes
     * @return The nodes
     */
    const std::vector<std::shared_ptr<Node>>& getNodes() const override;

    /**
     * @brief Adds a node
     * @param node The node to add
     */
    void addNode(std::shared_ptr<Node> node);

    /**
     * @brief Sets the nodes
     * @param nodes The nodes
     */
    void setNodes(const std::vector<std::shared_ptr<Node>>& nodes);

    /**
     * @brief Gets the peers
     * @return The peers
     */
    std::optional<std::vector<network::EndPoint>> getPeers() const;

    /**
     * @brief Adds a peer
     * @param peer The peer to add
     */
    void addPeer(const network::EndPoint& peer);

    /**
     * @brief Sets the peers
     * @param peers The peers
     */
    void setPeers(const std::vector<network::EndPoint>& peers);

protected:
    /**
     * @brief Encodes the get_peers response values to a bencode dictionary
     * @param dict The bencode dictionary to add the values to
     */
    void encodeValues(bencode::BencodeDict& dict) const override;

    /**
     * @brief Decodes the get_peers response values from a bencode dictionary
     * @param dict The bencode dictionary to get the values from
     * @return True if decoding was successful, false otherwise
     */
    bool decodeValues(const bencode::BencodeDict& dict) override;

private:
    std::string m_token;
    std::vector<std::shared_ptr<Node>> m_nodes;
    std::vector<network::EndPoint> m_peers;
};

/**
 * @brief DHT announce_peer response message
 */
class AnnouncePeerResponse : public ResponseMessage {
public:
    /**
     * @brief Constructs an announce_peer response message
     * @param transactionID The transaction ID
     * @param nodeID The node ID of the sender
     */
    AnnouncePeerResponse(const std::string& transactionID = "", const NodeID& nodeID = NodeID());

protected:
    /**
     * @brief Encodes the announce_peer response values to a bencode dictionary
     * @param dict The bencode dictionary to add the values to
     */
    void encodeValues(bencode::BencodeDict& dict) const override;

    /**
     * @brief Decodes the announce_peer response values from a bencode dictionary
     * @param dict The bencode dictionary to get the values from
     * @return True if decoding was successful, false otherwise
     */
    bool decodeValues(const bencode::BencodeDict& dict) override;
};

} // namespace dht_hunter::dht
