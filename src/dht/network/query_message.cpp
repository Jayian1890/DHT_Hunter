#include "dht_hunter/dht/network/query_message.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"
#include <sstream>

namespace dht_hunter::dht {

QueryMessage::QueryMessage(const std::string& transactionID, const NodeID& nodeID, QueryMethod method)
    : Message(transactionID, nodeID), m_method(method) {
}

MessageType QueryMessage::getType() const {
    return MessageType::Query;
}

QueryMethod QueryMessage::getMethod() const {
    return m_method;
}

std::vector<uint8_t> QueryMessage::encode() const {
    // Create the dictionary
    auto dict = std::make_shared<dht_hunter::bencode::BencodeValue>();
    dht_hunter::bencode::BencodeValue::Dictionary emptyDict;
    dict->setDict(emptyDict);

    // Add the transaction ID
    dict->setString("t", m_transactionID);

    // Add the message type
    dict->setString("y", "q");

    // Add the query method
    dict->setString("q", getMethodName());

    // Get the arguments
    auto args = getArguments();

    // Add the node ID to the arguments
    if (m_nodeID) {
        std::string nodeIDStr(reinterpret_cast<const char*>(m_nodeID->data()), m_nodeID->size());
        args->setString("id", nodeIDStr);
    }

    // Add the arguments to the dictionary
    dict->set("a", args);

    // Add the client version if available
    if (!m_clientVersion.empty()) {
        dict->setString("v", m_clientVersion);
    }

    // Encode the dictionary
    std::string encoded = dht_hunter::bencode::BencodeEncoder::encode(dict);

    // Convert to byte vector
    return std::vector<uint8_t>(encoded.begin(), encoded.end());
}

std::shared_ptr<QueryMessage> QueryMessage::decode(const dht_hunter::bencode::BencodeValue& value) {    // Logger initialization removed

    // Check if the value is a dictionary
    if (!value.isDictionary()) {
        return nullptr;
    }

    // Check if the dictionary has a query method
    auto methodName = value.getString("q");
    if (!methodName) {
        return nullptr;
    }

    // Check if the dictionary has arguments
    auto argsDict = value.getDict("a");
    if (!argsDict) {
        return nullptr;
    }

    // Create a BencodeValue from the Dictionary
    dht_hunter::bencode::BencodeValue args(*argsDict);

    // Check if the arguments have a node ID
    auto nodeIDStr = args.getString("id");
    if (!nodeIDStr || nodeIDStr->size() != 20) {
        return nullptr;
    }

    // Convert the node ID
    NodeID nodeID;
    std::copy(nodeIDStr->begin(), nodeIDStr->end(), nodeID.begin());

    // Get the transaction ID
    auto transactionID = value.getString("t");
    if (!transactionID) {
        return nullptr;
    }

    // Create the query message based on the method
    if (*methodName == "ping") {
        return PingQuery::create(*transactionID, nodeID, args);
    } else if (*methodName == "find_node") {
        return FindNodeQuery::create(*transactionID, nodeID, args);
    } else if (*methodName == "get_peers") {
        return GetPeersQuery::create(*transactionID, nodeID, args);
    } else if (*methodName == "announce_peer") {
        return AnnouncePeerQuery::create(*transactionID, nodeID, args);
    } else {
        return nullptr;
    }
}

PingQuery::PingQuery(const std::string& transactionID, const NodeID& nodeID)
    : QueryMessage(transactionID, nodeID, QueryMethod::Ping) {
}

std::shared_ptr<PingQuery> PingQuery::create(const std::string& transactionID, const NodeID& nodeID, const dht_hunter::bencode::BencodeValue& /*arguments*/) {
    return std::make_shared<PingQuery>(transactionID, nodeID);
}

std::string PingQuery::getMethodName() const {
    return "ping";
}

std::shared_ptr<dht_hunter::bencode::BencodeValue> PingQuery::getArguments() const {
    auto args = std::make_shared<dht_hunter::bencode::BencodeValue>();
    dht_hunter::bencode::BencodeValue::Dictionary emptyDict;
    args->setDict(emptyDict);
    return args;
}

FindNodeQuery::FindNodeQuery(const std::string& transactionID, const NodeID& nodeID, const NodeID& targetID)
    : QueryMessage(transactionID, nodeID, QueryMethod::FindNode), m_targetID(targetID) {
}

const NodeID& FindNodeQuery::getTargetID() const {
    return m_targetID;
}

std::shared_ptr<FindNodeQuery> FindNodeQuery::create(const std::string& transactionID, const NodeID& nodeID, const dht_hunter::bencode::BencodeValue& arguments) {    // Logger initialization removed

    // Check if the arguments have a target ID
    auto targetIDStr = arguments.getString("target");
    if (!targetIDStr || targetIDStr->size() != 20) {
        return nullptr;
    }

    // Convert the target ID
    NodeID targetID;
    std::copy(targetIDStr->begin(), targetIDStr->end(), targetID.begin());

    return std::make_shared<FindNodeQuery>(transactionID, nodeID, targetID);
}

std::string FindNodeQuery::getMethodName() const {
    return "find_node";
}

std::shared_ptr<dht_hunter::bencode::BencodeValue> FindNodeQuery::getArguments() const {
    auto args = std::make_shared<dht_hunter::bencode::BencodeValue>();
    dht_hunter::bencode::BencodeValue::Dictionary emptyDict;
    args->setDict(emptyDict);

    // Add the target ID
    std::string targetIDStr(reinterpret_cast<const char*>(m_targetID.data()), m_targetID.size());
    args->setString("target", targetIDStr);

    return args;
}

GetPeersQuery::GetPeersQuery(const std::string& transactionID, const NodeID& nodeID, const InfoHash& infoHash)
    : QueryMessage(transactionID, nodeID, QueryMethod::GetPeers), m_infoHash(infoHash) {
}

const InfoHash& GetPeersQuery::getInfoHash() const {
    return m_infoHash;
}

std::shared_ptr<GetPeersQuery> GetPeersQuery::create(const std::string& transactionID, const NodeID& nodeID, const dht_hunter::bencode::BencodeValue& arguments) {    // Logger initialization removed

    // Check if the arguments have an info hash
    auto infoHashStr = arguments.getString("info_hash");
    if (!infoHashStr || infoHashStr->size() != 20) {
        return nullptr;
    }

    // Convert the info hash
    InfoHash infoHash;
    std::copy(infoHashStr->begin(), infoHashStr->end(), infoHash.begin());

    return std::make_shared<GetPeersQuery>(transactionID, nodeID, infoHash);
}

std::string GetPeersQuery::getMethodName() const {
    return "get_peers";
}

std::shared_ptr<dht_hunter::bencode::BencodeValue> GetPeersQuery::getArguments() const {
    auto args = std::make_shared<dht_hunter::bencode::BencodeValue>();
    dht_hunter::bencode::BencodeValue::Dictionary emptyDict;
    args->setDict(emptyDict);

    // Add the info hash
    std::string infoHashStr(reinterpret_cast<const char*>(m_infoHash.data()), m_infoHash.size());
    args->setString("info_hash", infoHashStr);

    return args;
}

AnnouncePeerQuery::AnnouncePeerQuery(const std::string& transactionID, const NodeID& nodeID, const InfoHash& infoHash, uint16_t port, const std::string& token, bool impliedPort)
    : QueryMessage(transactionID, nodeID, QueryMethod::AnnouncePeer), m_infoHash(infoHash), m_port(port), m_token(token), m_impliedPort(impliedPort) {
}

const InfoHash& AnnouncePeerQuery::getInfoHash() const {
    return m_infoHash;
}

uint16_t AnnouncePeerQuery::getPort() const {
    return m_port;
}

const std::string& AnnouncePeerQuery::getToken() const {
    return m_token;
}

bool AnnouncePeerQuery::isImpliedPort() const {
    return m_impliedPort;
}

std::shared_ptr<AnnouncePeerQuery> AnnouncePeerQuery::create(const std::string& transactionID, const NodeID& nodeID, const dht_hunter::bencode::BencodeValue& arguments) {    // Logger initialization removed

    // Check if the arguments have an info hash
    auto infoHashStr = arguments.getString("info_hash");
    if (!infoHashStr || infoHashStr->size() != 20) {
        return nullptr;
    }

    // Convert the info hash
    InfoHash infoHash;
    std::copy(infoHashStr->begin(), infoHashStr->end(), infoHash.begin());

    // Check if the arguments have a port
    uint16_t port = 0;
    bool impliedPort = false;

    // Check if the implied_port flag is set
    auto impliedPortVal = arguments.getInteger("implied_port");
    if (impliedPortVal) {
        impliedPort = *impliedPortVal != 0;
    }

    // If implied_port is not set, check for the port
    if (!impliedPort) {
        auto portVal = arguments.getInteger("port");
        if (!portVal) {
            return nullptr;
        }

        // Get the port
        port = static_cast<uint16_t>(*portVal);
    }

    // Check if the arguments have a token
    auto tokenVal = arguments.getString("token");
    if (!tokenVal) {
        return nullptr;
    }

    // Get the token
    std::string token = *tokenVal;

    return std::make_shared<AnnouncePeerQuery>(transactionID, nodeID, infoHash, port, token, impliedPort);
}

std::string AnnouncePeerQuery::getMethodName() const {
    return "announce_peer";
}

std::shared_ptr<dht_hunter::bencode::BencodeValue> AnnouncePeerQuery::getArguments() const {
    auto args = std::make_shared<dht_hunter::bencode::BencodeValue>();
    dht_hunter::bencode::BencodeValue::Dictionary emptyDict;
    args->setDict(emptyDict);

    // Add the info hash
    std::string infoHashStr(reinterpret_cast<const char*>(m_infoHash.data()), m_infoHash.size());
    args->setString("info_hash", infoHashStr);

    // Add the port or implied_port
    if (m_impliedPort) {
        args->setInteger("implied_port", 1);
    } else {
        args->setInteger("port", static_cast<int64_t>(m_port));
    }

    // Add the token
    args->setString("token", m_token);

    return args;
}

} // namespace dht_hunter::dht
