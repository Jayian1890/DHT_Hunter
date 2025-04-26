#include "dht_hunter/dht/network/message.hpp"
#include "dht_hunter/dht/network/query_message.hpp"
#include "dht_hunter/dht/network/response_message.hpp"
#include "dht_hunter/dht/network/error_message.hpp"
#include "dht_hunter/event/logger.hpp"
#include "dht_hunter/bencode/bencode.hpp"

// Alias for convenience
using BencodeDict = dht_hunter::bencode::BencodeValue::Dictionary;
using BencodeValue = std::shared_ptr<dht_hunter::bencode::BencodeValue>;
using BencodeList = dht_hunter::bencode::BencodeValue::List;

namespace dht_hunter::dht {

Message::Message(const std::string& transactionID, const std::optional<NodeID>& nodeID)
    : m_transactionID(transactionID), m_nodeID(nodeID) {
}

const std::string& Message::getTransactionID() const {
    return m_transactionID;
}

void Message::setTransactionID(const std::string& transactionID) {
    m_transactionID = transactionID;
}

const std::optional<NodeID>& Message::getNodeID() const {
    return m_nodeID;
}

void Message::setNodeID(const NodeID& nodeID) {
    m_nodeID = nodeID;
}

std::shared_ptr<Message> Message::decode(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        return nullptr;
    }

    auto logger = event::Logger::forComponent("DHT.Message");

    try {
        // Decode the bencode data
        std::string dataStr(reinterpret_cast<const char*>(data), size);
        size_t pos = 0;
        auto bencodeData = dht_hunter::bencode::BencodeDecoder::decode(dataStr, pos);

        // Check if we decoded the entire message
        if (pos != dataStr.size()) {
            logger.error("Decoded only " + std::to_string(pos) + " of " + std::to_string(dataStr.size()) + " bytes");
        }

        // Check if the data is a dictionary
        if (!bencodeData->isDictionary()) {
            logger.error("Decoded data is not a dictionary");
            return nullptr;
        }

        // Check if the dictionary has a transaction ID
        auto tValue = bencodeData->getString("t");
        if (!tValue) {
            logger.error("Dictionary does not have a valid transaction ID");
            return nullptr;
        }

        // Get the transaction ID
        std::string transactionID = *tValue;

        // Check if the dictionary has a message type
        auto yValue = bencodeData->getString("y");
        if (!yValue) {
            logger.error("Dictionary does not have a valid message type");
            return nullptr;
        }

        // Get the message type
        std::string messageType = *yValue;

        // Decode the message based on its type
        if (messageType == "q") {
            // Query message
            return QueryMessage::decode(*bencodeData);
        } else if (messageType == "r") {
            // Response message
            return ResponseMessage::decode(*bencodeData);
        } else if (messageType == "e") {
            // Error message
            return ErrorMessage::decode(*bencodeData);
        } else {
            logger.error("Unknown message type: " + messageType);
            return nullptr;
        }
    } catch (const std::exception& e) {
        logger.error("Failed to decode message: " + std::string(e.what()));
        return nullptr;
    }
}

} // namespace dht_hunter::dht
