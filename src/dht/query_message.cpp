#include "dht_hunter/dht/query_message.hpp"
#include "dht_hunter/logforge/logforge.hpp"

namespace dht_hunter::dht {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("DHT.QueryMessage");
}

// QueryMessage implementation

QueryMessage::QueryMessage(QueryMethod method, const TransactionID& transactionID, const NodeID& nodeID)
    : Message(MessageType::Query, transactionID), m_method(method), m_nodeID(nodeID) {
}

QueryMethod QueryMessage::getMethod() const {
    return m_method;
}

const NodeID& QueryMessage::getNodeID() const {
    return m_nodeID;
}

std::shared_ptr<bencode::BencodeValue> QueryMessage::encode() const {
    auto message = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::Dictionary());
    
    // Set message type
    message->setString("y", "q");
    
    // Set query method
    message->setString("q", queryMethodToString(m_method));
    
    // Set transaction ID
    message->setString("t", std::string(m_transactionID.begin(), m_transactionID.end()));
    
    // Set arguments
    message->set("a", encodeArguments());
    
    return message;
}

std::shared_ptr<bencode::BencodeValue> QueryMessage::encodeArguments() const {
    auto args = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::Dictionary());
    
    // Set node ID
    args->setString("id", std::string(m_nodeID.begin(), m_nodeID.end()));
    
    return args;
}

std::shared_ptr<QueryMessage> QueryMessage::decode(const bencode::BencodeValue& value) {
    // Check if it's a query
    auto q = value.getString("q");
    if (!q) {
        logger->warning("Query has no 'q' field");
        return nullptr;
    }
    
    // Get transaction ID
    auto t = value.getString("t");
    if (!t) {
        logger->warning("Query has no 't' field");
        return nullptr;
    }
    
    TransactionID transactionID(t->begin(), t->end());
    
    // Get arguments
    auto a = value.get("a");
    if (!a || !a->isDictionary()) {
        logger->warning("Query has no 'a' field or it's not a dictionary");
        return nullptr;
    }
    
    // Get node ID
    auto id = a->getString("id");
    if (!id || id->length() != 20) {
        logger->warning("Query has no 'id' field or it's not 20 bytes");
        return nullptr;
    }
    
    NodeID nodeID;
    std::copy(id->begin(), id->end(), nodeID.begin());
    
    // Create the appropriate query based on the method
    QueryMethod method = stringToQueryMethod(*q);
    
    switch (method) {
        case QueryMethod::Ping:
            return PingQuery::decode(value);
        case QueryMethod::FindNode:
            return FindNodeQuery::decode(value);
        case QueryMethod::GetPeers:
            return GetPeersQuery::decode(value);
        case QueryMethod::AnnouncePeer:
            return AnnouncePeerQuery::decode(value);
        default:
            logger->warning("Unknown query method: {}", *q);
            return nullptr;
    }
}

// PingQuery implementation

PingQuery::PingQuery(const TransactionID& transactionID, const NodeID& nodeID)
    : QueryMessage(QueryMethod::Ping, transactionID, nodeID) {
}

std::shared_ptr<PingQuery> PingQuery::decode(const bencode::BencodeValue& value) {
    // Check if it's a ping query
    auto q = value.getString("q");
    if (!q || *q != "ping") {
        logger->warning("Not a ping query");
        return nullptr;
    }
    
    // Get transaction ID
    auto t = value.getString("t");
    if (!t) {
        logger->warning("Ping query has no 't' field");
        return nullptr;
    }
    
    TransactionID transactionID(t->begin(), t->end());
    
    // Get arguments
    auto a = value.get("a");
    if (!a || !a->isDictionary()) {
        logger->warning("Ping query has no 'a' field or it's not a dictionary");
        return nullptr;
    }
    
    // Get node ID
    auto id = a->getString("id");
    if (!id || id->length() != 20) {
        logger->warning("Ping query has no 'id' field or it's not 20 bytes");
        return nullptr;
    }
    
    NodeID nodeID;
    std::copy(id->begin(), id->end(), nodeID.begin());
    
    return std::make_shared<PingQuery>(transactionID, nodeID);
}

// FindNodeQuery implementation

FindNodeQuery::FindNodeQuery(const TransactionID& transactionID, const NodeID& nodeID, const NodeID& target)
    : QueryMessage(QueryMethod::FindNode, transactionID, nodeID), m_target(target) {
}

const NodeID& FindNodeQuery::getTarget() const {
    return m_target;
}

std::shared_ptr<bencode::BencodeValue> FindNodeQuery::encodeArguments() const {
    auto args = QueryMessage::encodeArguments();
    
    // Set target
    args->setString("target", std::string(m_target.begin(), m_target.end()));
    
    return args;
}

std::shared_ptr<FindNodeQuery> FindNodeQuery::decode(const bencode::BencodeValue& value) {
    // Check if it's a find_node query
    auto q = value.getString("q");
    if (!q || *q != "find_node") {
        logger->warning("Not a find_node query");
        return nullptr;
    }
    
    // Get transaction ID
    auto t = value.getString("t");
    if (!t) {
        logger->warning("find_node query has no 't' field");
        return nullptr;
    }
    
    TransactionID transactionID(t->begin(), t->end());
    
    // Get arguments
    auto a = value.get("a");
    if (!a || !a->isDictionary()) {
        logger->warning("find_node query has no 'a' field or it's not a dictionary");
        return nullptr;
    }
    
    // Get node ID
    auto id = a->getString("id");
    if (!id || id->length() != 20) {
        logger->warning("find_node query has no 'id' field or it's not 20 bytes");
        return nullptr;
    }
    
    NodeID nodeID;
    std::copy(id->begin(), id->end(), nodeID.begin());
    
    // Get target
    auto target = a->getString("target");
    if (!target || target->length() != 20) {
        logger->warning("find_node query has no 'target' field or it's not 20 bytes");
        return nullptr;
    }
    
    NodeID targetID;
    std::copy(target->begin(), target->end(), targetID.begin());
    
    return std::make_shared<FindNodeQuery>(transactionID, nodeID, targetID);
}

// GetPeersQuery implementation

GetPeersQuery::GetPeersQuery(const TransactionID& transactionID, const NodeID& nodeID, const InfoHash& infoHash)
    : QueryMessage(QueryMethod::GetPeers, transactionID, nodeID), m_infoHash(infoHash) {
}

const InfoHash& GetPeersQuery::getInfoHash() const {
    return m_infoHash;
}

std::shared_ptr<bencode::BencodeValue> GetPeersQuery::encodeArguments() const {
    auto args = QueryMessage::encodeArguments();
    
    // Set info_hash
    args->setString("info_hash", std::string(m_infoHash.begin(), m_infoHash.end()));
    
    return args;
}

std::shared_ptr<GetPeersQuery> GetPeersQuery::decode(const bencode::BencodeValue& value) {
    // Check if it's a get_peers query
    auto q = value.getString("q");
    if (!q || *q != "get_peers") {
        logger->warning("Not a get_peers query");
        return nullptr;
    }
    
    // Get transaction ID
    auto t = value.getString("t");
    if (!t) {
        logger->warning("get_peers query has no 't' field");
        return nullptr;
    }
    
    TransactionID transactionID(t->begin(), t->end());
    
    // Get arguments
    auto a = value.get("a");
    if (!a || !a->isDictionary()) {
        logger->warning("get_peers query has no 'a' field or it's not a dictionary");
        return nullptr;
    }
    
    // Get node ID
    auto id = a->getString("id");
    if (!id || id->length() != 20) {
        logger->warning("get_peers query has no 'id' field or it's not 20 bytes");
        return nullptr;
    }
    
    NodeID nodeID;
    std::copy(id->begin(), id->end(), nodeID.begin());
    
    // Get info_hash
    auto infoHash = a->getString("info_hash");
    if (!infoHash || infoHash->length() != 20) {
        logger->warning("get_peers query has no 'info_hash' field or it's not 20 bytes");
        return nullptr;
    }
    
    InfoHash hash;
    std::copy(infoHash->begin(), infoHash->end(), hash.begin());
    
    return std::make_shared<GetPeersQuery>(transactionID, nodeID, hash);
}

// AnnouncePeerQuery implementation

AnnouncePeerQuery::AnnouncePeerQuery(const TransactionID& transactionID, const NodeID& nodeID, 
                                   const InfoHash& infoHash, uint16_t port, 
                                   const std::string& token, bool impliedPort)
    : QueryMessage(QueryMethod::AnnouncePeer, transactionID, nodeID), 
      m_infoHash(infoHash), m_port(port), m_token(token), m_impliedPort(impliedPort) {
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

std::shared_ptr<bencode::BencodeValue> AnnouncePeerQuery::encodeArguments() const {
    auto args = QueryMessage::encodeArguments();
    
    // Set info_hash
    args->setString("info_hash", std::string(m_infoHash.begin(), m_infoHash.end()));
    
    // Set port
    args->setInteger("port", m_port);
    
    // Set token
    args->setString("token", m_token);
    
    // Set implied_port if needed
    if (m_impliedPort) {
        args->setInteger("implied_port", 1);
    }
    
    return args;
}

std::shared_ptr<AnnouncePeerQuery> AnnouncePeerQuery::decode(const bencode::BencodeValue& value) {
    // Check if it's an announce_peer query
    auto q = value.getString("q");
    if (!q || *q != "announce_peer") {
        logger->warning("Not an announce_peer query");
        return nullptr;
    }
    
    // Get transaction ID
    auto t = value.getString("t");
    if (!t) {
        logger->warning("announce_peer query has no 't' field");
        return nullptr;
    }
    
    TransactionID transactionID(t->begin(), t->end());
    
    // Get arguments
    auto a = value.get("a");
    if (!a || !a->isDictionary()) {
        logger->warning("announce_peer query has no 'a' field or it's not a dictionary");
        return nullptr;
    }
    
    // Get node ID
    auto id = a->getString("id");
    if (!id || id->length() != 20) {
        logger->warning("announce_peer query has no 'id' field or it's not 20 bytes");
        return nullptr;
    }
    
    NodeID nodeID;
    std::copy(id->begin(), id->end(), nodeID.begin());
    
    // Get info_hash
    auto infoHash = a->getString("info_hash");
    if (!infoHash || infoHash->length() != 20) {
        logger->warning("announce_peer query has no 'info_hash' field or it's not 20 bytes");
        return nullptr;
    }
    
    InfoHash hash;
    std::copy(infoHash->begin(), infoHash->end(), hash.begin());
    
    // Get token
    auto token = a->getString("token");
    if (!token) {
        logger->warning("announce_peer query has no 'token' field");
        return nullptr;
    }
    
    // Check for implied_port
    bool impliedPort = false;
    auto impliedPortValue = a->getInteger("implied_port");
    if (impliedPortValue && *impliedPortValue == 1) {
        impliedPort = true;
    }
    
    // Get port
    uint16_t port = 0;
    if (!impliedPort) {
        auto portValue = a->getInteger("port");
        if (!portValue) {
            logger->warning("announce_peer query has no 'port' field");
            return nullptr;
        }
        
        port = static_cast<uint16_t>(*portValue);
    }
    
    return std::make_shared<AnnouncePeerQuery>(transactionID, nodeID, hash, port, *token, impliedPort);
}

} // namespace dht_hunter::dht
