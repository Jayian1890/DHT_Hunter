#include "dht_hunter/dht/response_message.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"

DEFINE_COMPONENT_LOGGER("DHT", "ResponseMessage")

namespace dht_hunter::dht {
// ResponseMessage implementation
ResponseMessage::ResponseMessage(const TransactionID& transactionID, const NodeID& nodeID)
    : Message(MessageType::Response, transactionID), m_nodeID(nodeID) {
}
const NodeID& ResponseMessage::getNodeID() const {
    return m_nodeID;
}
std::shared_ptr<bencode::BencodeValue> ResponseMessage::encode() const {
    auto message = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::Dictionary());
    // Set message type
    message->setString("y", "r");
    // Set transaction ID
    message->setString("t", std::string(m_transactionID.begin(), m_transactionID.end()));
    // Set response values
    message->set("r", encodeValues());
    return message;
}
std::shared_ptr<bencode::BencodeValue> ResponseMessage::encodeValues() const {
    auto values = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::Dictionary());
    // Set node ID
    values->setString("id", std::string(m_nodeID.begin(), m_nodeID.end()));
    return values;
}
std::shared_ptr<ResponseMessage> ResponseMessage::decode(const bencode::BencodeValue& value) {
    // Get transaction ID
    auto t = value.getString("t");
    if (!t) {
        getLogger()->warning("Response has no 't' field");
        return nullptr;
    }
    TransactionID transactionID(t->begin(), t->end());
    // Get response values
    auto r = value.get("r");
    if (!r || !r->isDictionary()) {
        getLogger()->warning("Response has no 'r' field or it's not a dictionary");
        return nullptr;
    }
    // Get node ID
    auto id = r->getString("id");
    if (!id || id->length() != 20) {
        getLogger()->warning("Response has no 'id' field or it's not 20 bytes");
        return nullptr;
    }
    NodeID nodeID;
    std::copy(id->begin(), id->end(), nodeID.begin());
    // Check for specific response types
    if (r->getString("nodes")) {
        return FindNodeResponse::decode(value);
    } else if (r->getString("token")) {
        return GetPeersResponse::decode(value);
    } else {
        // Default to ping response if no specific fields are found
        return PingResponse::decode(value);
    }
}
// PingResponse implementation
PingResponse::PingResponse(const TransactionID& transactionID, const NodeID& nodeID)
    : ResponseMessage(transactionID, nodeID) {
}
std::shared_ptr<PingResponse> PingResponse::decode(const bencode::BencodeValue& value) {
    // Get transaction ID
    auto t = value.getString("t");
    if (!t) {
        getLogger()->warning("Ping response has no 't' field");
        return nullptr;
    }
    TransactionID transactionID(t->begin(), t->end());
    // Get response values
    auto r = value.get("r");
    if (!r || !r->isDictionary()) {
        getLogger()->warning("Ping response has no 'r' field or it's not a dictionary");
        return nullptr;
    }
    // Get node ID
    auto id = r->getString("id");
    if (!id || id->length() != 20) {
        getLogger()->warning("Ping response has no 'id' field or it's not 20 bytes");
        return nullptr;
    }
    NodeID nodeID;
    std::copy(id->begin(), id->end(), nodeID.begin());
    return std::make_shared<PingResponse>(transactionID, nodeID);
}
// FindNodeResponse implementation
FindNodeResponse::FindNodeResponse(const TransactionID& transactionID, const NodeID& nodeID,
                                 const std::vector<CompactNodeInfo>& nodes)
    : ResponseMessage(transactionID, nodeID), m_nodes(nodes) {
}
const std::vector<CompactNodeInfo>& FindNodeResponse::getNodes() const {
    return m_nodes;
}
std::shared_ptr<bencode::BencodeValue> FindNodeResponse::encodeValues() const {
    auto values = ResponseMessage::encodeValues();
    // Set nodes
    values->setString("nodes", encodeCompactNodeInfo(m_nodes));
    return values;
}
std::shared_ptr<FindNodeResponse> FindNodeResponse::decode(const bencode::BencodeValue& value) {
    // Get transaction ID
    auto t = value.getString("t");
    if (!t) {
        getLogger()->warning("find_node response has no 't' field");
        return nullptr;
    }
    TransactionID transactionID(t->begin(), t->end());
    // Get response values
    auto r = value.get("r");
    if (!r || !r->isDictionary()) {
        getLogger()->warning("find_node response has no 'r' field or it's not a dictionary");
        return nullptr;
    }
    // Get node ID
    auto id = r->getString("id");
    if (!id || id->length() != 20) {
        getLogger()->warning("find_node response has no 'id' field or it's not 20 bytes");
        return nullptr;
    }
    NodeID nodeID;
    std::copy(id->begin(), id->end(), nodeID.begin());
    // Get nodes
    auto nodes = r->getString("nodes");
    if (!nodes) {
        getLogger()->warning("find_node response has no 'nodes' field");
        return nullptr;
    }
    auto nodeInfos = parseCompactNodeInfo(*nodes);
    return std::make_shared<FindNodeResponse>(transactionID, nodeID, nodeInfos);
}
// GetPeersResponse implementation
GetPeersResponse::GetPeersResponse(const TransactionID& transactionID, const NodeID& nodeID,
                                 const std::vector<CompactNodeInfo>& nodes, const std::string& token)
    : ResponseMessage(transactionID, nodeID), m_nodes(nodes), m_token(token),
      m_hasNodes(true), m_hasPeers(false) {
}
GetPeersResponse::GetPeersResponse(const TransactionID& transactionID, const NodeID& nodeID,
                                 const std::vector<network::EndPoint>& peers, const std::string& token)
    : ResponseMessage(transactionID, nodeID), m_peers(peers), m_token(token),
      m_hasNodes(false), m_hasPeers(true) {
}
const std::vector<CompactNodeInfo>& GetPeersResponse::getNodes() const {
    return m_nodes;
}
const std::vector<network::EndPoint>& GetPeersResponse::getPeers() const {
    return m_peers;
}
const std::string& GetPeersResponse::getToken() const {
    return m_token;
}
bool GetPeersResponse::hasNodes() const {
    return m_hasNodes;
}
bool GetPeersResponse::hasPeers() const {
    return m_hasPeers;
}
std::shared_ptr<bencode::BencodeValue> GetPeersResponse::encodeValues() const {
    auto values = ResponseMessage::encodeValues();
    // Set token
    values->setString("token", m_token);
    // Set nodes or peers
    if (m_hasNodes) {
        values->setString("nodes", encodeCompactNodeInfo(m_nodes));
    } else if (m_hasPeers) {
        auto peersList = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::List());
        for (const auto& peer : m_peers) {
            // Encode peer as compact peer info (6 bytes: 4 bytes IP + 2 bytes port)
            std::string peerInfo;
            // Append IP address (4 bytes)
            uint32_t ipv4 = htonl(peer.getAddress().getIPv4Address());
            peerInfo.append(reinterpret_cast<const char*>(&ipv4), 4);
            // Append port (2 bytes)
            uint16_t port = htons(peer.getPort());
            peerInfo.append(reinterpret_cast<const char*>(&port), 2);
            peersList->addString(peerInfo);
        }
        values->set("values", peersList);
    }
    return values;
}
std::shared_ptr<GetPeersResponse> GetPeersResponse::decode(const bencode::BencodeValue& value) {
    // Get transaction ID
    auto t = value.getString("t");
    if (!t) {
        getLogger()->warning("get_peers response has no 't' field");
        return nullptr;
    }
    TransactionID transactionID(t->begin(), t->end());
    // Get response values
    auto r = value.get("r");
    if (!r || !r->isDictionary()) {
        getLogger()->warning("get_peers response has no 'r' field or it's not a dictionary");
        return nullptr;
    }
    // Get node ID
    auto id = r->getString("id");
    if (!id || id->length() != 20) {
        getLogger()->warning("get_peers response has no 'id' field or it's not 20 bytes");
        return nullptr;
    }
    NodeID nodeID;
    std::copy(id->begin(), id->end(), nodeID.begin());
    // Get token
    auto token = r->getString("token");
    if (!token) {
        getLogger()->warning("get_peers response has no 'token' field");
        return nullptr;
    }
    // Check if response has nodes or peers
    auto nodes = r->getString("nodes");
    auto values = r->get("values");
    if (nodes) {
        // Response with nodes
        auto nodeInfos = parseCompactNodeInfo(*nodes);
        return std::make_shared<GetPeersResponse>(transactionID, nodeID, nodeInfos, *token);
    } else if (values && values->isList()) {
        // Response with peers
        std::vector<network::EndPoint> peers;
        for (const auto& peerValue : values->getList()) {
            if (!peerValue->isString()) {
                getLogger()->warning("Peer value is not a string");
                continue;
            }
            const auto& peerInfo = peerValue->getString();
            // Each peer info is 6 bytes: 4 bytes IP + 2 bytes port
            if (peerInfo.length() != 6) {
                getLogger()->warning("Invalid peer info length: {}", peerInfo.length());
                continue;
            }
            // Extract IP address (4 bytes)
            uint32_t ipv4;
            std::memcpy(&ipv4, peerInfo.data(), 4);
            // Extract port (2 bytes)
            uint16_t port;
            std::memcpy(&port, peerInfo.data() + 4, 2);
            port = ntohs(port);
            // Create endpoint
            peers.emplace_back(network::NetworkAddress(ntohl(ipv4)), port);
        }
        return std::make_shared<GetPeersResponse>(transactionID, nodeID, peers, *token);
    } else {
        getLogger()->warning("get_peers response has neither 'nodes' nor 'values' field");
        return nullptr;
    }
}
// AnnouncePeerResponse implementation
AnnouncePeerResponse::AnnouncePeerResponse(const TransactionID& transactionID, const NodeID& nodeID)
    : ResponseMessage(transactionID, nodeID) {
}
std::shared_ptr<AnnouncePeerResponse> AnnouncePeerResponse::decode(const bencode::BencodeValue& value) {
    // Get transaction ID
    auto t = value.getString("t");
    if (!t) {
        getLogger()->warning("announce_peer response has no 't' field");
        return nullptr;
    }
    TransactionID transactionID(t->begin(), t->end());
    // Get response values
    auto r = value.get("r");
    if (!r || !r->isDictionary()) {
        getLogger()->warning("announce_peer response has no 'r' field or it's not a dictionary");
        return nullptr;
    }
    // Get node ID
    auto id = r->getString("id");
    if (!id || id->length() != 20) {
        getLogger()->warning("announce_peer response has no 'id' field or it's not 20 bytes");
        return nullptr;
    }
    NodeID nodeID;
    std::copy(id->begin(), id->end(), nodeID.begin());
    return std::make_shared<AnnouncePeerResponse>(transactionID, nodeID);
}
} // namespace dht_hunter::dht
