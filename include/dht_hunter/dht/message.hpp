#pragma once

#include "dht_hunter/dht/types.hpp"
#include "dht_hunter/bencode/bencode.hpp"

#include <string>
#include <memory>

namespace dht_hunter::dht {

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

// Forward declarations
class QueryMessage;
class ResponseMessage;
class ErrorMessage;

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
