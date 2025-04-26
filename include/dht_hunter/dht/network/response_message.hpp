#pragma once

#include "dht_hunter/dht/network/message.hpp"
#include <string>
#include <memory>
#include <vector>
#include "dht_hunter/bencode/bencode.hpp"

namespace dht_hunter::dht {

/**
 * @brief Base class for all DHT response messages
 */
class ResponseMessage : public Message {
public:
    /**
     * @brief Constructs a response message
     * @param transactionID The transaction ID
     * @param nodeID The node ID
     */
    ResponseMessage(const std::string& transactionID, const NodeID& nodeID);

    /**
     * @brief Gets the message type
     * @return The message type
     */
    MessageType getType() const override;

    /**
     * @brief Encodes the message to a byte vector
     * @return The encoded message
     */
    std::vector<uint8_t> encode() const override;

    /**
     * @brief Decodes a response message from a bencode value
     * @param value The bencode value
     * @return The decoded response message, or nullptr if decoding failed
     */
    static std::shared_ptr<ResponseMessage> decode(const dht_hunter::bencode::BencodeValue& value);

    /**
     * @brief Gets the sender's IP address
     * @return The sender's IP address, or empty string if not available
     */
    const std::string& getSenderIP() const;

    /**
     * @brief Sets the sender's IP address
     * @param senderIP The sender's IP address
     */
    void setSenderIP(const std::string& senderIP);

protected:
    /**
     * @brief Gets the response values
     * @return The response values
     */
    virtual std::shared_ptr<dht_hunter::bencode::BencodeValue> getResponseValues() const = 0;

    std::string m_senderIP; // Optional sender IP address ("ip" field)
};

/**
 * @brief DHT ping response
 */
class PingResponse : public ResponseMessage {
public:
    /**
     * @brief Constructs a ping response
     * @param transactionID The transaction ID
     * @param nodeID The node ID
     */
    PingResponse(const std::string& transactionID, const NodeID& nodeID);

    /**
     * @brief Creates a ping response from a bencode dictionary
     * @param transactionID The transaction ID
     * @param nodeID The node ID
     * @param responseValues The response values
     * @return The ping response
     */
    static std::shared_ptr<PingResponse> create(const std::string& transactionID, const NodeID& nodeID, const dht_hunter::bencode::BencodeValue& responseValues);

protected:
    /**
     * @brief Gets the response values
     * @return The response values
     */
    std::shared_ptr<dht_hunter::bencode::BencodeValue> getResponseValues() const override;
};

/**
 * @brief DHT find_node response
 */
class FindNodeResponse : public ResponseMessage {
public:
    /**
     * @brief Constructs a find_node response
     * @param transactionID The transaction ID
     * @param nodeID The node ID
     * @param nodes The nodes
     */
    FindNodeResponse(const std::string& transactionID, const NodeID& nodeID, const std::vector<std::shared_ptr<Node>>& nodes);

    /**
     * @brief Gets the nodes
     * @return The nodes
     */
    const std::vector<std::shared_ptr<Node>>& getNodes() const;

    /**
     * @brief Creates a find_node response from a bencode dictionary
     * @param transactionID The transaction ID
     * @param nodeID The node ID
     * @param responseValues The response values
     * @return The find_node response
     */
    static std::shared_ptr<FindNodeResponse> create(const std::string& transactionID, const NodeID& nodeID, const dht_hunter::bencode::BencodeValue& responseValues);

protected:
    /**
     * @brief Gets the response values
     * @return The response values
     */
    std::shared_ptr<dht_hunter::bencode::BencodeValue> getResponseValues() const override;

private:
    std::vector<std::shared_ptr<Node>> m_nodes;
};

/**
 * @brief DHT get_peers response
 */
class GetPeersResponse : public ResponseMessage {
public:
    /**
     * @brief Constructs a get_peers response with nodes
     * @param transactionID The transaction ID
     * @param nodeID The node ID
     * @param nodes The nodes
     * @param token The token
     */
    GetPeersResponse(const std::string& transactionID, const NodeID& nodeID, const std::vector<std::shared_ptr<Node>>& nodes, const std::string& token);

    /**
     * @brief Constructs a get_peers response with peers
     * @param transactionID The transaction ID
     * @param nodeID The node ID
     * @param peers The peers
     * @param token The token
     */
    GetPeersResponse(const std::string& transactionID, const NodeID& nodeID, const std::vector<network::EndPoint>& peers, const std::string& token);

    /**
     * @brief Gets the nodes
     * @return The nodes
     */
    const std::vector<std::shared_ptr<Node>>& getNodes() const;

    /**
     * @brief Gets the peers
     * @return The peers
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
     * @brief Creates a get_peers response from a bencode dictionary
     * @param transactionID The transaction ID
     * @param nodeID The node ID
     * @param responseValues The response values
     * @return The get_peers response
     */
    static std::shared_ptr<GetPeersResponse> create(const std::string& transactionID, const NodeID& nodeID, const dht_hunter::bencode::BencodeValue& responseValues);

protected:
    /**
     * @brief Gets the response values
     * @return The response values
     */
    std::shared_ptr<dht_hunter::bencode::BencodeValue> getResponseValues() const override;

private:
    std::vector<std::shared_ptr<Node>> m_nodes;
    std::vector<network::EndPoint> m_peers;
    std::string m_token;
    bool m_hasNodes;
    bool m_hasPeers;
};

/**
 * @brief DHT announce_peer response
 */
class AnnouncePeerResponse : public ResponseMessage {
public:
    /**
     * @brief Constructs an announce_peer response
     * @param transactionID The transaction ID
     * @param nodeID The node ID
     */
    AnnouncePeerResponse(const std::string& transactionID, const NodeID& nodeID);

    /**
     * @brief Creates an announce_peer response from a bencode dictionary
     * @param transactionID The transaction ID
     * @param nodeID The node ID
     * @param responseValues The response values
     * @return The announce_peer response
     */
    static std::shared_ptr<AnnouncePeerResponse> create(const std::string& transactionID, const NodeID& nodeID, const dht_hunter::bencode::BencodeValue& responseValues);

protected:
    /**
     * @brief Gets the response values
     * @return The response values
     */
    std::shared_ptr<dht_hunter::bencode::BencodeValue> getResponseValues() const override;
};

} // namespace dht_hunter::dht
