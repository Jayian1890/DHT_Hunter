#pragma once

#include "dht_hunter/dht/network/message.hpp"
#include <string>
#include <memory>
#include <vector>
#include <bencode.hpp>

namespace dht_hunter::dht {

/**
 * @brief DHT error codes
 */
enum class ErrorCode {
    GenericError = 201,
    ServerError = 202,
    ProtocolError = 203,
    MethodUnknown = 204
};

/**
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
    ErrorMessage(const std::string& transactionID, ErrorCode code, const std::string& message);

    /**
     * @brief Gets the message type
     * @return The message type
     */
    MessageType getType() const override;

    /**
     * @brief Gets the error code
     * @return The error code
     */
    ErrorCode getCode() const;

    /**
     * @brief Gets the error message
     * @return The error message
     */
    const std::string& getMessage() const;

    /**
     * @brief Encodes the message to a byte vector
     * @return The encoded message
     */
    std::vector<uint8_t> encode() const override;

    /**
     * @brief Decodes an error message from a bencode dictionary
     * @param dict The bencode dictionary
     * @return The decoded error message, or nullptr if decoding failed
     */
    static std::shared_ptr<ErrorMessage> decode(const bencode::dict& dict);

private:
    ErrorCode m_code;
    std::string m_message;
};

} // namespace dht_hunter::dht
