#include "dht_hunter/dht/error_message.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"

DEFINE_COMPONENT_LOGGER("DHT", "ErrorMessage")

namespace dht_hunter::dht {
ErrorMessage::ErrorMessage(const TransactionID& transactionID, int code, const std::string& message)
    : Message(MessageType::Error, transactionID), m_code(code), m_message(message) {
}
int ErrorMessage::getCode() const {
    return m_code;
}
const std::string& ErrorMessage::getMessage() const {
    return m_message;
}
std::shared_ptr<bencode::BencodeValue> ErrorMessage::encode() const {
    auto message = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::Dictionary());
    // Set message type
    message->setString("y", "e");
    // Set transaction ID
    message->setString("t", std::string(m_transactionID.begin(), m_transactionID.end()));
    // Set error
    auto error = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::List());
    error->addInteger(m_code);
    error->addString(m_message);
    message->set("e", error);
    return message;
}
std::shared_ptr<ErrorMessage> ErrorMessage::decode(const bencode::BencodeValue& value) {
    // Get transaction ID
    auto t = value.getString("t");
    if (!t) {
        getLogger()->warning("Error message has no 't' field");
        return nullptr;
    }
    TransactionID transactionID(t->begin(), t->end());
    // Get error
    auto e = value.get("e");
    if (!e || !e->isList()) {
        getLogger()->warning("Error message has no 'e' field or it's not a list");
        return nullptr;
    }
    const auto& errorList = e->getList();
    if (errorList.size() < 2) {
        getLogger()->warning("Error list has less than 2 elements");
        return nullptr;
    }
    // Get error code
    if (!errorList[0]->isInteger()) {
        getLogger()->warning("Error code is not an integer");
        return nullptr;
    }
    int code = static_cast<int>(errorList[0]->getInteger());
    // Get error message
    if (!errorList[1]->isString()) {
        getLogger()->warning("Error message is not a string");
        return nullptr;
    }
    std::string message = errorList[1]->getString();
    return std::make_shared<ErrorMessage>(transactionID, code, message);
}
} // namespace dht_hunter::dht
