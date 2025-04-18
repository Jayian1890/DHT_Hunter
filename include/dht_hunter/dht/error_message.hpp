#pragma once

#include "dht_hunter/dht/message.hpp"

namespace dht_hunter::dht {

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

} // namespace dht_hunter::dht
