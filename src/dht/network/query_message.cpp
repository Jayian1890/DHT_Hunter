#include "dht_hunter/dht/network/query_message.hpp"
#include "dht_hunter/event/logger.hpp"
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
    bencode::dict dict;
    
    // Add the transaction ID
    dict["t"] = m_transactionID;
    
    // Add the message type
    dict["y"] = "q";
    
    // Add the query method
    dict["q"] = getMethodName();
    
    // Add the arguments
    bencode::dict args = getArguments();
    
    // Add the node ID to the arguments
    if (m_nodeID) {
        std::string nodeIDStr(reinterpret_cast<const char*>(m_nodeID->data()), m_nodeID->size());
        args["id"] = nodeIDStr;
    }
    
    dict["a"] = args;
    
    // Encode the dictionary
    std::string encoded = bencode::encode(dict);
    
    // Convert to byte vector
    return std::vector<uint8_t>(encoded.begin(), encoded.end());
}

std::shared_ptr<QueryMessage> QueryMessage::decode(const bencode::dict& dict) {
    auto logger = event::Logger::forComponent("DHT.QueryMessage");
    
    // Check if the dictionary has a query method
    auto qIt = dict.find("q");
    if (qIt == dict.end() || !std::holds_alternative<bencode::string>(qIt->second)) {
        logger.error("Dictionary does not have a valid query method");
        return nullptr;
    }
    
    // Get the query method
    std::string methodName = std::get<bencode::string>(qIt->second);
    
    // Check if the dictionary has arguments
    auto aIt = dict.find("a");
    if (aIt == dict.end() || !std::holds_alternative<bencode::dict>(aIt->second)) {
        logger.error("Dictionary does not have valid arguments");
        return nullptr;
    }
    
    // Get the arguments
    const auto& args = std::get<bencode::dict>(aIt->second);
    
    // Check if the arguments have a node ID
    auto idIt = args.find("id");
    if (idIt == args.end() || !std::holds_alternative<bencode::string>(idIt->second)) {
        logger.error("Arguments do not have a valid node ID");
        return nullptr;
    }
    
    // Get the node ID
    const auto& nodeIDStr = std::get<bencode::string>(idIt->second);
    if (nodeIDStr.size() != 20) {
        logger.error("Node ID has invalid size: {}", nodeIDStr.size());
        return nullptr;
    }
    
    // Convert the node ID
    NodeID nodeID;
    std::copy(nodeIDStr.begin(), nodeIDStr.end(), nodeID.begin());
    
    // Get the transaction ID
    auto tIt = dict.find("t");
    if (tIt == dict.end() || !std::holds_alternative<bencode::string>(tIt->second)) {
        logger.error("Dictionary does not have a valid transaction ID");
        return nullptr;
    }
    
    std::string transactionID = std::get<bencode::string>(tIt->second);
    
    // Create the query message based on the method
    if (methodName == "ping") {
        return PingQuery::create(transactionID, nodeID, args);
    } else if (methodName == "find_node") {
        return FindNodeQuery::create(transactionID, nodeID, args);
    } else if (methodName == "get_peers") {
        return GetPeersQuery::create(transactionID, nodeID, args);
    } else if (methodName == "announce_peer") {
        return AnnouncePeerQuery::create(transactionID, nodeID, args);
    } else {
        logger.error("Unknown query method: {}", methodName);
        return nullptr;
    }
}

PingQuery::PingQuery(const std::string& transactionID, const NodeID& nodeID)
    : QueryMessage(transactionID, nodeID, QueryMethod::Ping) {
}

std::shared_ptr<PingQuery> PingQuery::create(const std::string& transactionID, const NodeID& nodeID, const bencode::dict& arguments) {
    return std::make_shared<PingQuery>(transactionID, nodeID);
}

std::string PingQuery::getMethodName() const {
    return "ping";
}

bencode::dict PingQuery::getArguments() const {
    return {};
}

FindNodeQuery::FindNodeQuery(const std::string& transactionID, const NodeID& nodeID, const NodeID& targetID)
    : QueryMessage(transactionID, nodeID, QueryMethod::FindNode), m_targetID(targetID) {
}

const NodeID& FindNodeQuery::getTargetID() const {
    return m_targetID;
}

std::shared_ptr<FindNodeQuery> FindNodeQuery::create(const std::string& transactionID, const NodeID& nodeID, const bencode::dict& arguments) {
    auto logger = event::Logger::forComponent("DHT.FindNodeQuery");
    
    // Check if the arguments have a target ID
    auto targetIt = arguments.find("target");
    if (targetIt == arguments.end() || !std::holds_alternative<bencode::string>(targetIt->second)) {
        logger.error("Arguments do not have a valid target ID");
        return nullptr;
    }
    
    // Get the target ID
    const auto& targetIDStr = std::get<bencode::string>(targetIt->second);
    if (targetIDStr.size() != 20) {
        logger.error("Target ID has invalid size: {}", targetIDStr.size());
        return nullptr;
    }
    
    // Convert the target ID
    NodeID targetID;
    std::copy(targetIDStr.begin(), targetIDStr.end(), targetID.begin());
    
    return std::make_shared<FindNodeQuery>(transactionID, nodeID, targetID);
}

std::string FindNodeQuery::getMethodName() const {
    return "find_node";
}

bencode::dict FindNodeQuery::getArguments() const {
    bencode::dict args;
    
    // Add the target ID
    std::string targetIDStr(reinterpret_cast<const char*>(m_targetID.data()), m_targetID.size());
    args["target"] = targetIDStr;
    
    return args;
}

GetPeersQuery::GetPeersQuery(const std::string& transactionID, const NodeID& nodeID, const InfoHash& infoHash)
    : QueryMessage(transactionID, nodeID, QueryMethod::GetPeers), m_infoHash(infoHash) {
}

const InfoHash& GetPeersQuery::getInfoHash() const {
    return m_infoHash;
}

std::shared_ptr<GetPeersQuery> GetPeersQuery::create(const std::string& transactionID, const NodeID& nodeID, const bencode::dict& arguments) {
    auto logger = event::Logger::forComponent("DHT.GetPeersQuery");
    
    // Check if the arguments have an info hash
    auto infoHashIt = arguments.find("info_hash");
    if (infoHashIt == arguments.end() || !std::holds_alternative<bencode::string>(infoHashIt->second)) {
        logger.error("Arguments do not have a valid info hash");
        return nullptr;
    }
    
    // Get the info hash
    const auto& infoHashStr = std::get<bencode::string>(infoHashIt->second);
    if (infoHashStr.size() != 20) {
        logger.error("Info hash has invalid size: {}", infoHashStr.size());
        return nullptr;
    }
    
    // Convert the info hash
    InfoHash infoHash;
    std::copy(infoHashStr.begin(), infoHashStr.end(), infoHash.begin());
    
    return std::make_shared<GetPeersQuery>(transactionID, nodeID, infoHash);
}

std::string GetPeersQuery::getMethodName() const {
    return "get_peers";
}

bencode::dict GetPeersQuery::getArguments() const {
    bencode::dict args;
    
    // Add the info hash
    std::string infoHashStr(reinterpret_cast<const char*>(m_infoHash.data()), m_infoHash.size());
    args["info_hash"] = infoHashStr;
    
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

std::shared_ptr<AnnouncePeerQuery> AnnouncePeerQuery::create(const std::string& transactionID, const NodeID& nodeID, const bencode::dict& arguments) {
    auto logger = event::Logger::forComponent("DHT.AnnouncePeerQuery");
    
    // Check if the arguments have an info hash
    auto infoHashIt = arguments.find("info_hash");
    if (infoHashIt == arguments.end() || !std::holds_alternative<bencode::string>(infoHashIt->second)) {
        logger.error("Arguments do not have a valid info hash");
        return nullptr;
    }
    
    // Get the info hash
    const auto& infoHashStr = std::get<bencode::string>(infoHashIt->second);
    if (infoHashStr.size() != 20) {
        logger.error("Info hash has invalid size: {}", infoHashStr.size());
        return nullptr;
    }
    
    // Convert the info hash
    InfoHash infoHash;
    std::copy(infoHashStr.begin(), infoHashStr.end(), infoHash.begin());
    
    // Check if the arguments have a port
    uint16_t port = 0;
    bool impliedPort = false;
    
    // Check if the implied_port flag is set
    auto impliedPortIt = arguments.find("implied_port");
    if (impliedPortIt != arguments.end() && std::holds_alternative<bencode::integer>(impliedPortIt->second)) {
        impliedPort = std::get<bencode::integer>(impliedPortIt->second) != 0;
    }
    
    // If implied_port is not set, check for the port
    if (!impliedPort) {
        auto portIt = arguments.find("port");
        if (portIt == arguments.end() || !std::holds_alternative<bencode::integer>(portIt->second)) {
            logger.error("Arguments do not have a valid port");
            return nullptr;
        }
        
        // Get the port
        port = static_cast<uint16_t>(std::get<bencode::integer>(portIt->second));
    }
    
    // Check if the arguments have a token
    auto tokenIt = arguments.find("token");
    if (tokenIt == arguments.end() || !std::holds_alternative<bencode::string>(tokenIt->second)) {
        logger.error("Arguments do not have a valid token");
        return nullptr;
    }
    
    // Get the token
    std::string token = std::get<bencode::string>(tokenIt->second);
    
    return std::make_shared<AnnouncePeerQuery>(transactionID, nodeID, infoHash, port, token, impliedPort);
}

std::string AnnouncePeerQuery::getMethodName() const {
    return "announce_peer";
}

bencode::dict AnnouncePeerQuery::getArguments() const {
    bencode::dict args;
    
    // Add the info hash
    std::string infoHashStr(reinterpret_cast<const char*>(m_infoHash.data()), m_infoHash.size());
    args["info_hash"] = infoHashStr;
    
    // Add the port or implied_port
    if (m_impliedPort) {
        args["implied_port"] = bencode::integer(1);
    } else {
        args["port"] = bencode::integer(m_port);
    }
    
    // Add the token
    args["token"] = m_token;
    
    return args;
}

} // namespace dht_hunter::dht
