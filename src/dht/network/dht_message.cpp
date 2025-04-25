#include "dht_hunter/dht/network/dht_message.hpp"
#include "dht_hunter/dht/network/dht_query_message.hpp"
#include "dht_hunter/dht/network/dht_response_message.hpp"
#include "dht_hunter/dht/network/dht_error_message.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"

DEFINE_COMPONENT_LOGGER("DHT", "Message")

namespace dht_hunter::dht {

DHTMessage::DHTMessage(const std::string& transactionID)
    : m_transactionID(transactionID) {
}

const std::string& DHTMessage::getTransactionID() const {
    return m_transactionID;
}

void DHTMessage::setTransactionID(const std::string& transactionID) {
    m_transactionID = transactionID;
}

std::string DHTMessage::encodeToString() const {
    auto dict = encode();
    return bencode::encode(dict);
}

std::shared_ptr<DHTMessage> DHTMessage::decode(const bencode::BencodeDict& dict) {
    // Check if the dictionary has a transaction ID
    auto transactionIDIt = dict.find("t");
    if (transactionIDIt == dict.end() || !transactionIDIt->second->isString()) {
        getLogger()->error("Message does not have a valid transaction ID");
        return nullptr;
    }
    std::string transactionID = transactionIDIt->second->getString();

    // Check if the dictionary has a message type
    auto typeIt = dict.find("y");
    if (typeIt == dict.end() || !typeIt->second->isString()) {
        getLogger()->error("Message does not have a valid type");
        return nullptr;
    }
    std::string typeStr = typeIt->second->getString();

    // Decode the message based on its type
    if (typeStr == "q") {
        // Query message
        return QueryMessage::decode(dict);
    } else if (typeStr == "r") {
        // Response message
        // TODO: We need to know the query method to decode the response
        // This is not available in the response message itself
        // So we need to look it up in the transaction manager
        // For now, we'll just return a generic response message
        return nullptr;
    } else if (typeStr == "e") {
        // Error message
        return ErrorMessage::decode(dict);
    } else {
        getLogger()->error("Unknown message type: {}", typeStr);
        return nullptr;
    }
}

} // namespace dht_hunter::dht
