#include "dht_hunter/dht/network/dht_query_message.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"

DEFINE_COMPONENT_LOGGER("DHT", "QueryMessage")

namespace dht_hunter::dht {

std::string queryMethodToString(QueryMethod method) {
    switch (method) {
        case QueryMethod::Ping:
            return "ping";
        case QueryMethod::FindNode:
            return "find_node";
        case QueryMethod::GetPeers:
            return "get_peers";
        case QueryMethod::AnnouncePeer:
            return "announce_peer";
        default:
            return "unknown";
    }
}

QueryMessage::QueryMessage(const std::string& transactionID, QueryMethod method, const NodeID& nodeID)
    : DHTMessage(transactionID), m_method(method), m_nodeID(nodeID) {
}

MessageType QueryMessage::getType() const {
    return MessageType::Query;
}

QueryMethod QueryMessage::getMethod() const {
    return m_method;
}

const NodeID& QueryMessage::getNodeIDRef() const {
    return m_nodeID;
}

std::optional<NodeID> QueryMessage::getNodeID() const {
    return m_nodeID;
}

void QueryMessage::setNodeID(const NodeID& nodeID) {
    m_nodeID = nodeID;
}

bencode::BencodeDict QueryMessage::encode() const {
    bencode::BencodeDict dict;

    // Add the transaction ID
    auto tValue = std::make_shared<bencode::BencodeValue>();
    tValue->setString(m_transactionID);
    dict["t"] = tValue;

    // Add the message type
    auto yValue = std::make_shared<bencode::BencodeValue>();
    yValue->setString("q");
    dict["y"] = yValue;

    // Add the query method
    auto qValue = std::make_shared<bencode::BencodeValue>();
    switch (m_method) {
        case QueryMethod::Ping:
            qValue->setString("ping");
            break;
        case QueryMethod::FindNode:
            qValue->setString("find_node");
            break;
        case QueryMethod::GetPeers:
            qValue->setString("get_peers");
            break;
        case QueryMethod::AnnouncePeer:
            qValue->setString("announce_peer");
            break;
        default:
            getLogger()->error("Unknown query method: {}", static_cast<int>(m_method));
            qValue->setString("ping");
            break;
    }
    dict["q"] = qValue;

    // Add the arguments
    bencode::BencodeDict argsDict;

    // Add the node ID
    auto idValue = std::make_shared<bencode::BencodeValue>();
    idValue->setString(nodeIDToString(m_nodeID));
    argsDict["id"] = idValue;

    // Add the query-specific arguments
    encodeArguments(argsDict);

    // Add the arguments to the dictionary
    auto aValue = std::make_shared<bencode::BencodeValue>();
    aValue->setDict(argsDict);
    dict["a"] = aValue;

    return dict;
}

std::shared_ptr<QueryMessage> QueryMessage::decode(const bencode::BencodeDict& dict) {
    // Check if the dictionary has a transaction ID
    auto transactionIDIt = dict.find("t");
    if (transactionIDIt == dict.end() || !transactionIDIt->second->isString()) {
        getLogger()->error("Query message does not have a valid transaction ID");
        return nullptr;
    }
    std::string transactionID = transactionIDIt->second->getString();

    // Check if the dictionary has a query method
    auto queryMethodIt = dict.find("q");
    if (queryMethodIt == dict.end() || !queryMethodIt->second->isString()) {
        getLogger()->error("Query message does not have a valid query method");
        return nullptr;
    }
    std::string queryMethodStr = queryMethodIt->second->getString();

    // Check if the dictionary has arguments
    auto argsIt = dict.find("a");
    if (argsIt == dict.end() || !argsIt->second->isDictionary()) {
        getLogger()->error("Query message does not have valid arguments");
        return nullptr;
    }
    const auto& argsDict = argsIt->second->getDictionary();

    // Check if the arguments have a node ID
    auto nodeIDIt = argsDict.find("id");
    if (nodeIDIt == argsDict.end() || !nodeIDIt->second->isString()) {
        getLogger()->error("Query message arguments do not have a valid node ID");
        return nullptr;
    }
    auto nodeIDOpt = stringToNodeID(nodeIDIt->second->getString());
    if (!nodeIDOpt) {
        getLogger()->error("Query message arguments have an invalid node ID");
        return nullptr;
    }
    NodeID nodeID = nodeIDOpt.value();

    // Create the query message based on the query method
    std::shared_ptr<QueryMessage> query;
    if (queryMethodStr == "ping") {
        query = std::make_shared<PingQuery>(transactionID, nodeID);
    } else if (queryMethodStr == "find_node") {
        query = std::make_shared<FindNodeQuery>(transactionID, nodeID);
    } else if (queryMethodStr == "get_peers") {
        query = std::make_shared<GetPeersQuery>(transactionID, nodeID);
    } else if (queryMethodStr == "announce_peer") {
        query = std::make_shared<AnnouncePeerQuery>(transactionID, nodeID);
    } else {
        getLogger()->error("Unknown query method: {}", queryMethodStr);
        return nullptr;
    }

    // Decode the query-specific arguments
    if (!query->decodeArguments(argsDict)) {
        getLogger()->error("Failed to decode query-specific arguments");
        return nullptr;
    }

    return query;
}

PingQuery::PingQuery(const std::string& transactionID, const NodeID& nodeID)
    : QueryMessage(transactionID, QueryMethod::Ping, nodeID) {
}

void PingQuery::encodeArguments(bencode::BencodeDict& /*dict*/) const {
    // No additional arguments for ping query
}

bool PingQuery::decodeArguments(const bencode::BencodeDict& /*dict*/) {
    // No additional arguments for ping query
    return true;
}

FindNodeQuery::FindNodeQuery(const std::string& transactionID, const NodeID& nodeID, const NodeID& targetID)
    : QueryMessage(transactionID, QueryMethod::FindNode, nodeID), m_targetID(targetID) {
}

FindNodeQuery::FindNodeQuery(const NodeID& targetID)
    : QueryMessage("", QueryMethod::FindNode, NodeID()), m_targetID(targetID) {
}

const NodeID& FindNodeQuery::getTargetID() const {
    return m_targetID;
}

void FindNodeQuery::setTargetID(const NodeID& targetID) {
    m_targetID = targetID;
}

void FindNodeQuery::encodeArguments(bencode::BencodeDict& dict) const {
    // Add the target ID
    auto targetValue = std::make_shared<bencode::BencodeValue>();
    targetValue->setString(nodeIDToString(m_targetID));
    dict["target"] = targetValue;
}

bool FindNodeQuery::decodeArguments(const bencode::BencodeDict& dict) {
    // Check if the arguments have a target ID
    auto targetIt = dict.find("target");
    if (targetIt == dict.end() || !targetIt->second->isString()) {
        getLogger()->error("Find_node query arguments do not have a valid target ID");
        return false;
    }
    auto targetIDOpt = stringToNodeID(targetIt->second->getString());
    if (!targetIDOpt) {
        getLogger()->error("Find_node query arguments have an invalid target ID");
        return false;
    }
    m_targetID = targetIDOpt.value();
    return true;
}

GetPeersQuery::GetPeersQuery(const std::string& transactionID, const NodeID& nodeID, const InfoHash& infoHash)
    : QueryMessage(transactionID, QueryMethod::GetPeers, nodeID), m_infoHash(infoHash) {
}

GetPeersQuery::GetPeersQuery(const InfoHash& infoHash)
    : QueryMessage("", QueryMethod::GetPeers, NodeID()), m_infoHash(infoHash) {
}

const InfoHash& GetPeersQuery::getInfoHash() const {
    return m_infoHash;
}

void GetPeersQuery::setInfoHash(const InfoHash& infoHash) {
    m_infoHash = infoHash;
}

void GetPeersQuery::encodeArguments(bencode::BencodeDict& dict) const {
    // Add the info hash
    auto infoHashValue = std::make_shared<bencode::BencodeValue>();
    infoHashValue->setString(infoHashToString(m_infoHash));
    dict["info_hash"] = infoHashValue;
}

bool GetPeersQuery::decodeArguments(const bencode::BencodeDict& dict) {
    // Check if the arguments have an info hash
    auto infoHashIt = dict.find("info_hash");
    if (infoHashIt == dict.end() || !infoHashIt->second->isString()) {
        getLogger()->error("Get_peers query arguments do not have a valid info hash");
        return false;
    }
    auto infoHashOpt = stringToInfoHash(infoHashIt->second->getString());
    if (!infoHashOpt) {
        getLogger()->error("Get_peers query arguments have an invalid info hash");
        return false;
    }
    m_infoHash = infoHashOpt.value();
    return true;
}

AnnouncePeerQuery::AnnouncePeerQuery(const std::string& transactionID, const NodeID& nodeID, const InfoHash& infoHash, uint16_t port, bool implied_port)
    : QueryMessage(transactionID, QueryMethod::AnnouncePeer, nodeID), m_infoHash(infoHash), m_port(port), m_implied_port(implied_port) {
}

AnnouncePeerQuery::AnnouncePeerQuery(const InfoHash& infoHash, uint16_t port)
    : QueryMessage("", QueryMethod::AnnouncePeer, NodeID()), m_infoHash(infoHash), m_port(port), m_implied_port(false) {
}

const InfoHash& AnnouncePeerQuery::getInfoHash() const {
    return m_infoHash;
}

void AnnouncePeerQuery::setInfoHash(const InfoHash& infoHash) {
    m_infoHash = infoHash;
}

uint16_t AnnouncePeerQuery::getPort() const {
    return m_port;
}

void AnnouncePeerQuery::setPort(uint16_t port) {
    m_port = port;
}

bool AnnouncePeerQuery::getImpliedPort() const {
    return m_implied_port;
}

void AnnouncePeerQuery::setImpliedPort(bool implied_port) {
    m_implied_port = implied_port;
}

const std::string& AnnouncePeerQuery::getToken() const {
    return m_token;
}

void AnnouncePeerQuery::setToken(const std::string& token) {
    m_token = token;
}

void AnnouncePeerQuery::encodeArguments(bencode::BencodeDict& dict) const {
    // Add the info hash
    auto infoHashValue = std::make_shared<bencode::BencodeValue>();
    infoHashValue->setString(infoHashToString(m_infoHash));
    dict["info_hash"] = infoHashValue;

    // Add the port
    auto portValue = std::make_shared<bencode::BencodeValue>();
    portValue->setInteger(m_port);
    dict["port"] = portValue;

    // Add the implied_port flag if it's true
    if (m_implied_port) {
        auto impliedPortValue = std::make_shared<bencode::BencodeValue>();
        impliedPortValue->setInteger(1);
        dict["implied_port"] = impliedPortValue;
    }

    // Add the token
    auto tokenValue = std::make_shared<bencode::BencodeValue>();
    tokenValue->setString(m_token);
    dict["token"] = tokenValue;
}

bool AnnouncePeerQuery::decodeArguments(const bencode::BencodeDict& dict) {
    // Check if the arguments have an info hash
    auto infoHashIt = dict.find("info_hash");
    if (infoHashIt == dict.end() || !infoHashIt->second->isString()) {
        getLogger()->error("Announce_peer query arguments do not have a valid info hash");
        return false;
    }
    auto infoHashOpt = stringToInfoHash(infoHashIt->second->getString());
    if (!infoHashOpt) {
        getLogger()->error("Announce_peer query arguments have an invalid info hash");
        return false;
    }
    m_infoHash = infoHashOpt.value();

    // Check if the arguments have a port
    auto portIt = dict.find("port");
    if (portIt == dict.end() || !portIt->second->isInteger()) {
        getLogger()->error("Announce_peer query arguments do not have a valid port");
        return false;
    }
    m_port = static_cast<uint16_t>(portIt->second->getInteger());

    // Check if the arguments have an implied_port flag
    auto impliedPortIt = dict.find("implied_port");
    if (impliedPortIt != dict.end() && impliedPortIt->second->isInteger()) {
        m_implied_port = impliedPortIt->second->getInteger() != 0;
    } else {
        m_implied_port = false;
    }

    // Check if the arguments have a token
    auto tokenIt = dict.find("token");
    if (tokenIt == dict.end() || !tokenIt->second->isString()) {
        getLogger()->error("Announce_peer query arguments do not have a valid token");
        return false;
    }
    m_token = tokenIt->second->getString();

    return true;
}

} // namespace dht_hunter::dht
