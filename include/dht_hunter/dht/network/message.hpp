#pragma once

#include "dht_hunter/dht/types.hpp"
#include <string>
#include <memory>
#include <optional>
#include <vector>

namespace dht_hunter::dht {

/**
 * @brief Enum for the different types of DHT messages
 */
enum class MessageType {
    Query,
    Response,
    Error
};

/**
 * @brief Base class for all DHT messages
 */
class Message {
public:
    /**
     * @brief Constructs a message
     * @param transactionID The transaction ID
     * @param nodeID The node ID (optional)
     */
    Message(const std::string& transactionID, const std::optional<NodeID>& nodeID = std::nullopt);

    /**
     * @brief Virtual destructor
     */
    virtual ~Message() = default;

    /**
     * @brief Gets the transaction ID
     * @return The transaction ID
     */
    const std::string& getTransactionID() const;

    /**
     * @brief Sets the transaction ID
     * @param transactionID The transaction ID
     */
    void setTransactionID(const std::string& transactionID);

    /**
     * @brief Gets the node ID
     * @return The node ID
     */
    const std::optional<NodeID>& getNodeID() const;

    /**
     * @brief Sets the node ID
     * @param nodeID The node ID
     */
    void setNodeID(const NodeID& nodeID);

    /**
     * @brief Gets the message type
     * @return The message type
     */
    virtual MessageType getType() const = 0;

    /**
     * @brief Encodes the message to a byte vector
     * @return The encoded message
     */
    virtual std::vector<uint8_t> encode() const = 0;

    /**
     * @brief Decodes a message from a byte vector
     * @param data The data to decode
     * @param size The size of the data
     * @return The decoded message, or nullptr if decoding failed
     */
    static std::shared_ptr<Message> decode(const uint8_t* data, size_t size);

    /**
     * @brief Gets the client version
     * @return The client version, or empty string if not available
     */
    const std::string& getClientVersion() const;

    /**
     * @brief Sets the client version
     * @param clientVersion The client version
     */
    void setClientVersion(const std::string& clientVersion);

protected:
    std::string m_transactionID;
    std::optional<NodeID> m_nodeID;
    std::string m_clientVersion; // Optional client version ("v" field)
};

} // namespace dht_hunter::dht
