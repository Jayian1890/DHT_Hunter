#pragma once

#include "dht_hunter/dht/core/dht_types.hpp"
#include "dht_hunter/bencode/bencode.hpp"
#include "dht_hunter/network/network_address.hpp"
#include <string>
#include <memory>

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
class DHTMessage {
public:
    /**
     * @brief Constructs a message
     * @param transactionID The transaction ID
     */
    explicit DHTMessage(const std::string& transactionID);

    /**
     * @brief Destructor
     */
    virtual ~DHTMessage() = default;

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
     * @brief Gets the message type
     * @return The message type
     */
    virtual MessageType getType() const = 0;

    /**
     * @brief Gets the node ID of the sender
     * @return The node ID of the sender, if available
     */
    virtual std::optional<NodeID> getNodeID() const { return std::nullopt; }

    /**
     * @brief Encodes the message to a string
     * @return The encoded message
     */
    std::string encodeToString() const;

    /**
     * @brief Encodes the message to a bencode dictionary
     * @return The encoded message
     */
    virtual bencode::BencodeDict encode() const = 0;

    /**
     * @brief Decodes a message from a bencode dictionary
     * @param dict The bencode dictionary
     * @return The decoded message, or nullptr if decoding failed
     */
    static std::shared_ptr<DHTMessage> decode(const bencode::BencodeDict& dict);

protected:
    std::string m_transactionID;
};

} // namespace dht_hunter::dht
