#include "dht_hunter/dht/network/error_message.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"

namespace dht_hunter::dht {

ErrorMessage::ErrorMessage(const std::string& transactionID, ErrorCode code, const std::string& message)
    : Message(transactionID), m_code(code), m_message(message) {
}

MessageType ErrorMessage::getType() const {
    return MessageType::Error;
}

ErrorCode ErrorMessage::getCode() const {
    return m_code;
}

const std::string& ErrorMessage::getMessage() const {
    return m_message;
}

std::vector<uint8_t> ErrorMessage::encode() const {
    // Create the dictionary
    auto dict = std::make_shared<dht_hunter::bencode::BencodeValue>();
    dht_hunter::bencode::BencodeValue::Dictionary emptyDict;
    dict->setDict(emptyDict);

    // Add the transaction ID
    dict->setString("t", m_transactionID);

    // Add the message type
    dict->setString("y", "e");

    // Add the error
    auto error = std::make_shared<dht_hunter::bencode::BencodeValue>();
    error->setList(dht_hunter::bencode::BencodeValue::List());
    error->addInteger(static_cast<int64_t>(m_code));
    error->addString(m_message);

    dict->setList("e", error->getList());

    // Add the client version if available
    if (!m_clientVersion.empty()) {
        dict->setString("v", m_clientVersion);
    }

    // Encode the dictionary
    std::string encoded = dht_hunter::bencode::BencodeEncoder::encode(dict);

    // Convert to byte vector
    return std::vector<uint8_t>(encoded.begin(), encoded.end());
}

std::shared_ptr<ErrorMessage> ErrorMessage::decode(const dht_hunter::bencode::BencodeValue& value) {    // Logger initialization removed

    // Check if the value is a dictionary
    if (!value.isDictionary()) {
        return nullptr;
    }

    // Check if the dictionary has an error
    auto error = value.getList("e");
    if (!error) {
        return nullptr;
    }

    // Check if the error has the correct format
    if (error->size() != 2 || !error->at(0)->isInteger() || !error->at(1)->isString()) {
        return nullptr;
    }

    // Get the error code and message
    int code = static_cast<int>(error->at(0)->getInteger());
    std::string message = error->at(1)->getString();

    // Get the transaction ID
    auto transactionID = value.getString("t");
    if (!transactionID) {
        return nullptr;
    }

    return std::make_shared<ErrorMessage>(*transactionID, static_cast<ErrorCode>(code), message);
}

std::shared_ptr<ErrorMessage> ErrorMessage::createGenericError(const std::string& transactionID, const std::string& message) {
    return std::make_shared<ErrorMessage>(transactionID, ErrorCode::GenericError, message);
}

std::shared_ptr<ErrorMessage> ErrorMessage::createServerError(const std::string& transactionID, const std::string& message) {
    return std::make_shared<ErrorMessage>(transactionID, ErrorCode::ServerError, message);
}

std::shared_ptr<ErrorMessage> ErrorMessage::createProtocolError(const std::string& transactionID, const std::string& message) {
    return std::make_shared<ErrorMessage>(transactionID, ErrorCode::ProtocolError, message);
}

std::shared_ptr<ErrorMessage> ErrorMessage::createMethodUnknownError(const std::string& transactionID, const std::string& message) {
    return std::make_shared<ErrorMessage>(transactionID, ErrorCode::MethodUnknown, message);
}

std::shared_ptr<ErrorMessage> ErrorMessage::createInvalidTokenError(const std::string& transactionID, const std::string& message) {
    return std::make_shared<ErrorMessage>(transactionID, ErrorCode::InvalidToken, message);
}

std::shared_ptr<ErrorMessage> ErrorMessage::createInvalidArgumentError(const std::string& transactionID, const std::string& message) {
    return std::make_shared<ErrorMessage>(transactionID, ErrorCode::InvalidArgument, message);
}

std::shared_ptr<ErrorMessage> ErrorMessage::createInvalidNodeIDError(const std::string& transactionID, const std::string& message) {
    return std::make_shared<ErrorMessage>(transactionID, ErrorCode::InvalidNodeID, message);
}

std::shared_ptr<ErrorMessage> ErrorMessage::createInvalidInfoHashError(const std::string& transactionID, const std::string& message) {
    return std::make_shared<ErrorMessage>(transactionID, ErrorCode::InvalidInfoHash, message);
}

std::shared_ptr<ErrorMessage> ErrorMessage::createInvalidPortError(const std::string& transactionID, const std::string& message) {
    return std::make_shared<ErrorMessage>(transactionID, ErrorCode::InvalidPort, message);
}

std::string ErrorMessage::getErrorDescription(ErrorCode code) {
    switch (code) {
        case ErrorCode::GenericError:
            return "Generic error";
        case ErrorCode::ServerError:
            return "Server error";
        case ErrorCode::ProtocolError:
            return "Protocol error";
        case ErrorCode::MethodUnknown:
            return "Method unknown";
        case ErrorCode::InvalidToken:
            return "Invalid token";
        case ErrorCode::InvalidArgument:
            return "Invalid argument";
        case ErrorCode::InvalidNodeID:
            return "Invalid node ID";
        case ErrorCode::InvalidInfoHash:
            return "Invalid info hash";
        case ErrorCode::InvalidPort:
            return "Invalid port";
        case ErrorCode::InvalidIP:
            return "Invalid IP address";
        case ErrorCode::InvalidMessage:
            return "Invalid message format";
        case ErrorCode::InvalidSignature:
            return "Invalid signature";
        case ErrorCode::InvalidTransaction:
            return "Invalid transaction ID";
        default:
            return "Unknown error";
    }
}

} // namespace dht_hunter::dht
