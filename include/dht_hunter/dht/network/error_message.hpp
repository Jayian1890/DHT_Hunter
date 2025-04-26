#pragma once

#include "dht_hunter/dht/network/message.hpp"
#include <string>
#include <memory>
#include <vector>
#include "dht_hunter/bencode/bencode.hpp"

namespace dht_hunter::dht {

/**
 * @brief DHT error codes as defined in BEP-5
 */
enum class ErrorCode {
    // Generic errors (200-299)
    GenericError = 201,      // Generic error
    ServerError = 202,       // Server error
    ProtocolError = 203,     // Protocol error, such as a malformed packet, invalid arguments, or bad token
    MethodUnknown = 204,     // Method unknown

    // Specific errors (300-399)
    InvalidToken = 301,      // Invalid token
    InvalidArgument = 302,   // Invalid argument
    InvalidNodeID = 303,     // Invalid node ID
    InvalidInfoHash = 304,   // Invalid info hash
    InvalidPort = 305,       // Invalid port
    InvalidIP = 306,         // Invalid IP address
    InvalidMessage = 307,    // Invalid message format
    InvalidSignature = 308,  // Invalid signature
    InvalidTransaction = 309 // Invalid transaction ID
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
     * @brief Decodes an error message from a bencode value
     * @param value The bencode value
     * @return The decoded error message, or nullptr if decoding failed
     */
    static std::shared_ptr<ErrorMessage> decode(const dht_hunter::bencode::BencodeValue& value);

    /**
     * @brief Creates a generic error message
     * @param transactionID The transaction ID
     * @param message The error message
     * @return The error message
     */
    static std::shared_ptr<ErrorMessage> createGenericError(const std::string& transactionID, const std::string& message);

    /**
     * @brief Creates a server error message
     * @param transactionID The transaction ID
     * @param message The error message
     * @return The error message
     */
    static std::shared_ptr<ErrorMessage> createServerError(const std::string& transactionID, const std::string& message);

    /**
     * @brief Creates a protocol error message
     * @param transactionID The transaction ID
     * @param message The error message
     * @return The error message
     */
    static std::shared_ptr<ErrorMessage> createProtocolError(const std::string& transactionID, const std::string& message);

    /**
     * @brief Creates a method unknown error message
     * @param transactionID The transaction ID
     * @param message The error message
     * @return The error message
     */
    static std::shared_ptr<ErrorMessage> createMethodUnknownError(const std::string& transactionID, const std::string& message);

    /**
     * @brief Creates an invalid token error message
     * @param transactionID The transaction ID
     * @param message The error message
     * @return The error message
     */
    static std::shared_ptr<ErrorMessage> createInvalidTokenError(const std::string& transactionID, const std::string& message);

    /**
     * @brief Creates an invalid argument error message
     * @param transactionID The transaction ID
     * @param message The error message
     * @return The error message
     */
    static std::shared_ptr<ErrorMessage> createInvalidArgumentError(const std::string& transactionID, const std::string& message);

    /**
     * @brief Creates an invalid node ID error message
     * @param transactionID The transaction ID
     * @param message The error message
     * @return The error message
     */
    static std::shared_ptr<ErrorMessage> createInvalidNodeIDError(const std::string& transactionID, const std::string& message);

    /**
     * @brief Creates an invalid info hash error message
     * @param transactionID The transaction ID
     * @param message The error message
     * @return The error message
     */
    static std::shared_ptr<ErrorMessage> createInvalidInfoHashError(const std::string& transactionID, const std::string& message);

    /**
     * @brief Creates an invalid port error message
     * @param transactionID The transaction ID
     * @param message The error message
     * @return The error message
     */
    static std::shared_ptr<ErrorMessage> createInvalidPortError(const std::string& transactionID, const std::string& message);

    /**
     * @brief Gets a descriptive error message for an error code
     * @param code The error code
     * @return The descriptive error message
     */
    static std::string getErrorDescription(ErrorCode code);

private:
    ErrorCode m_code;
    std::string m_message;
};

} // namespace dht_hunter::dht
