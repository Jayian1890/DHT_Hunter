#include "dht_hunter/dht/network/dht_error_message.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"

DEFINE_COMPONENT_LOGGER("DHT", "ErrorMessage")

namespace dht_hunter::dht {

ErrorMessage::ErrorMessage(const std::string& transactionID, int code, const std::string& message)
    : DHTMessage(transactionID), m_code(code), m_message(message) {
}

MessageType ErrorMessage::getType() const {
    return MessageType::Error;
}

int ErrorMessage::getCode() const {
    return m_code;
}

void ErrorMessage::setCode(int code) {
    m_code = code;
}

const std::string& ErrorMessage::getMessage() const {
    return m_message;
}

void ErrorMessage::setMessage(const std::string& message) {
    m_message = message;
}

bencode::BencodeDict ErrorMessage::encode() const {
    bencode::BencodeDict dict;

    // Add the transaction ID
    auto tValue = std::make_shared<bencode::BencodeValue>();
    tValue->setString(m_transactionID);
    dict["t"] = tValue;

    // Add the message type
    auto yValue = std::make_shared<bencode::BencodeValue>();
    yValue->setString("e");
    dict["y"] = yValue;

    // Add the error
    bencode::BencodeList eList;

    // Add the error code
    auto codeValue = std::make_shared<bencode::BencodeValue>();
    codeValue->setInteger(m_code);
    eList.push_back(codeValue);

    // Add the error message
    auto messageValue = std::make_shared<bencode::BencodeValue>();
    messageValue->setString(m_message);
    eList.push_back(messageValue);

    // Add the error list to the dictionary
    auto eValue = std::make_shared<bencode::BencodeValue>();
    eValue->setList(eList);
    dict["e"] = eValue;

    return dict;
}

std::shared_ptr<ErrorMessage> ErrorMessage::decode(const bencode::BencodeDict& dict) {
    // Check if the dictionary has a transaction ID
    auto transactionIDIt = dict.find("t");
    if (transactionIDIt == dict.end() || !transactionIDIt->second->isString()) {
        getLogger()->error("Error message does not have a valid transaction ID");
        return nullptr;
    }
    std::string transactionID = transactionIDIt->second->getString();

    // Check if the dictionary has an error
    auto eIt = dict.find("e");
    if (eIt == dict.end() || !eIt->second->isList()) {
        getLogger()->error("Error message does not have a valid error");
        return nullptr;
    }
    const auto& eList = eIt->second->getList();

    // Check if the error list has at least two elements
    if (eList.size() < 2) {
        getLogger()->error("Error message error list does not have enough elements");
        return nullptr;
    }

    // Check if the first element is an integer (error code)
    if (!eList[0]->isInteger()) {
        getLogger()->error("Error message error code is not an integer");
        return nullptr;
    }
    int code = static_cast<int>(eList[0]->getInteger());

    // Check if the second element is a string (error message)
    if (!eList[1]->isString()) {
        getLogger()->error("Error message error message is not a string");
        return nullptr;
    }
    std::string message = eList[1]->getString();

    // Create the error message
    return std::make_shared<ErrorMessage>(transactionID, code, message);
}

} // namespace dht_hunter::dht
