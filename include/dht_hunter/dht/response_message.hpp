#pragma once

#include "dht_hunter/dht/message.hpp"
#include "dht_hunter/network/network_address.hpp"

namespace dht_hunter::dht {

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

} // namespace dht_hunter::dht
