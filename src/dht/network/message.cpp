#include "dht_hunter/dht/network/message.hpp"
#include "dht_hunter/dht/network/query_message.hpp"
#include "dht_hunter/dht/network/response_message.hpp"
#include "dht_hunter/dht/network/error_message.hpp"
#include "dht_hunter/event/logger.hpp"
#include <bencode.hpp>

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
        bencode::data bencodeData = bencode::decode({reinterpret_cast<const char*>(data), size});
        
        // Check if the data is a dictionary
        if (!std::holds_alternative<bencode::dict>(bencodeData)) {
            logger.error("Decoded data is not a dictionary");
            return nullptr;
        }
        
        // Get the dictionary
        const auto& dict = std::get<bencode::dict>(bencodeData);
        
        // Check if the dictionary has a transaction ID
        auto tIt = dict.find("t");
        if (tIt == dict.end() || !std::holds_alternative<bencode::string>(tIt->second)) {
            logger.error("Dictionary does not have a valid transaction ID");
            return nullptr;
        }
        
        // Get the transaction ID
        std::string transactionID = std::get<bencode::string>(tIt->second);
        
        // Check if the dictionary has a message type
        auto yIt = dict.find("y");
        if (yIt == dict.end() || !std::holds_alternative<bencode::string>(yIt->second)) {
            logger.error("Dictionary does not have a valid message type");
            return nullptr;
        }
        
        // Get the message type
        std::string messageType = std::get<bencode::string>(yIt->second);
        
        // Decode the message based on its type
        if (messageType == "q") {
            // Query message
            return QueryMessage::decode(dict);
        } else if (messageType == "r") {
            // Response message
            return ResponseMessage::decode(dict);
        } else if (messageType == "e") {
            // Error message
            return ErrorMessage::decode(dict);
        } else {
            logger.error("Unknown message type: {}", messageType);
            return nullptr;
        }
    } catch (const std::exception& e) {
        logger.error("Failed to decode message: {}", e.what());
        return nullptr;
    }
}

} // namespace dht_hunter::dht
