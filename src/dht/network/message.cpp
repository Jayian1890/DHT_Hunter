#include "dht_hunter/dht/network/message.hpp"
#include "dht_hunter/dht/network/query_message.hpp"
#include "dht_hunter/dht/network/response_message.hpp"
#include "dht_hunter/dht/network/error_message.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"
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

const std::string& Message::getClientVersion() const {
    return m_clientVersion;
}

void Message::setClientVersion(const std::string& clientVersion) {
    m_clientVersion = clientVersion;
}

std::shared_ptr<Message> Message::decode(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        return nullptr;
    }    // Logger initialization removed

    try {
        // Decode the bencode data
        std::string dataStr(reinterpret_cast<const char*>(data), size);
        size_t pos = 0;
        auto bencodeData = dht_hunter::bencode::BencodeDecoder::decode(dataStr, pos);

        // Check if we decoded the entire message
        if (pos != dataStr.size()) {
        }

        // Check if the data is a dictionary
        if (!bencodeData->isDictionary()) {
            return nullptr;
        }

        // Check if the dictionary has a transaction ID
        auto tValue = bencodeData->getString("t");
        if (!tValue) {
            return nullptr;
        }

        // Get the transaction ID
        std::string transactionID = *tValue;

        // Check if the dictionary has a message type
        auto yValue = bencodeData->getString("y");
        if (!yValue) {
            return nullptr;
        }

        // Get the message type
        std::string messageType = *yValue;

        // Check for client version (optional)
        std::string clientVersion;
        auto vValue = bencodeData->getString("v");
        if (vValue) {
            clientVersion = *vValue;
        }

        // Decode the message based on its type
        std::shared_ptr<Message> message;
        if (messageType == "q") {
            // Query message
            message = QueryMessage::decode(*bencodeData);
        } else if (messageType == "r") {
            // Response message
            message = ResponseMessage::decode(*bencodeData);
        } else if (messageType == "e") {
            // Error message
            message = ErrorMessage::decode(*bencodeData);
        } else {
            return nullptr;
        }

        // Set the client version if available
        if (message && !clientVersion.empty()) {
            message->setClientVersion(clientVersion);
        }

        return message;
    } catch (const std::exception& e) {
        return nullptr;
    }
}

} // namespace dht_hunter::dht
