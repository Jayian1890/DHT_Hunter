#include "dht_hunter/dht/message.hpp"
#include "dht_hunter/dht/query_message.hpp"
#include "dht_hunter/dht/response_message.hpp"
#include "dht_hunter/dht/error_message.hpp"
#include "dht_hunter/logforge/logforge.hpp"

namespace dht_hunter::dht {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("DHT.Message");
}

Message::Message(MessageType type, const TransactionID& transactionID)
    : m_type(type), m_transactionID(transactionID) {
}

MessageType Message::getType() const {
    return m_type;
}

const TransactionID& Message::getTransactionID() const {
    return m_transactionID;
}

std::string Message::encodeToString() const {
    auto value = encode();
    return bencode::BencodeEncoder::encode(*value);
}

std::shared_ptr<Message> decodeMessage(const bencode::BencodeValue& value) {
    if (!value.isDictionary()) {
        logger->warning("Message is not a dictionary");
        return nullptr;
    }
    
    // Check if it's a query, response, or error
    auto y = value.getString("y");
    if (!y) {
        logger->warning("Message has no 'y' field");
        return nullptr;
    }
    
    if (*y == "q") {
        // Query
        return QueryMessage::decode(value);
    } else if (*y == "r") {
        // Response
        return ResponseMessage::decode(value);
    } else if (*y == "e") {
        // Error
        return ErrorMessage::decode(value);
    } else {
        logger->warning("Unknown message type: {}", *y);
        return nullptr;
    }
}

std::shared_ptr<Message> decodeMessage(const std::string& data) {
    try {
        auto value = bencode::BencodeDecoder::decode(data);
        return decodeMessage(*value);
    } catch (const bencode::BencodeException& e) {
        logger->warning("Failed to decode message: {}", e.what());
        return nullptr;
    }
}

} // namespace dht_hunter::dht
