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

    // Encode the dictionary
    std::string encoded = dht_hunter::bencode::BencodeEncoder::encode(dict);

    // Convert to byte vector
    return std::vector<uint8_t>(encoded.begin(), encoded.end());
}

std::shared_ptr<ErrorMessage> ErrorMessage::decode(const dht_hunter::bencode::BencodeValue& value) {
    auto logger = event::Logger::forComponent("DHT.ErrorMessage");

    // Check if the value is a dictionary
    if (!value.isDictionary()) {
        logger.error("Value is not a dictionary");
        return nullptr;
    }

    // Check if the dictionary has an error
    auto error = value.getList("e");
    if (!error) {
        logger.error("Dictionary does not have a valid error");
        return nullptr;
    }

    // Check if the error has the correct format
    if (error->size() != 2 || !error->at(0)->isInteger() || !error->at(1)->isString()) {
        logger.error("Error does not have the correct format");
        return nullptr;
    }

    // Get the error code and message
    int code = static_cast<int>(error->at(0)->getInteger());
    std::string message = error->at(1)->getString();

    // Get the transaction ID
    auto transactionID = value.getString("t");
    if (!transactionID) {
        logger.error("Dictionary does not have a valid transaction ID");
        return nullptr;
    }

    return std::make_shared<ErrorMessage>(*transactionID, static_cast<ErrorCode>(code), message);
}

} // namespace dht_hunter::dht
