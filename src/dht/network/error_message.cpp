#include "dht_hunter/dht/network/error_message.hpp"
#include "dht_hunter/event/logger.hpp"

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
    bencode::dict dict;
    
    // Add the transaction ID
    dict["t"] = m_transactionID;
    
    // Add the message type
    dict["y"] = "e";
    
    // Add the error
    bencode::list error;
    error.push_back(bencode::integer(static_cast<int>(m_code)));
    error.push_back(m_message);
    
    dict["e"] = error;
    
    // Encode the dictionary
    std::string encoded = bencode::encode(dict);
    
    // Convert to byte vector
    return std::vector<uint8_t>(encoded.begin(), encoded.end());
}

std::shared_ptr<ErrorMessage> ErrorMessage::decode(const bencode::dict& dict) {
    auto logger = event::Logger::forComponent("DHT.ErrorMessage");
    
    // Check if the dictionary has an error
    auto eIt = dict.find("e");
    if (eIt == dict.end() || !std::holds_alternative<bencode::list>(eIt->second)) {
        logger.error("Dictionary does not have a valid error");
        return nullptr;
    }
    
    // Get the error
    const auto& error = std::get<bencode::list>(eIt->second);
    
    // Check if the error has the correct format
    if (error.size() != 2 || !std::holds_alternative<bencode::integer>(error[0]) || !std::holds_alternative<bencode::string>(error[1])) {
        logger.error("Error does not have the correct format");
        return nullptr;
    }
    
    // Get the error code and message
    int code = std::get<bencode::integer>(error[0]);
    std::string message = std::get<bencode::string>(error[1]);
    
    // Get the transaction ID
    auto tIt = dict.find("t");
    if (tIt == dict.end() || !std::holds_alternative<bencode::string>(tIt->second)) {
        logger.error("Dictionary does not have a valid transaction ID");
        return nullptr;
    }
    
    std::string transactionID = std::get<bencode::string>(tIt->second);
    
    return std::make_shared<ErrorMessage>(transactionID, static_cast<ErrorCode>(code), message);
}

} // namespace dht_hunter::dht
