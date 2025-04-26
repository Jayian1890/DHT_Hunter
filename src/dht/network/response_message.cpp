#include "dht_hunter/dht/network/response_message.hpp"
#include "dht_hunter/event/logger.hpp"
#include <sstream>

namespace dht_hunter::dht {

ResponseMessage::ResponseMessage(const std::string& transactionID, const NodeID& nodeID)
    : Message(transactionID, nodeID) {
}

MessageType ResponseMessage::getType() const {
    return MessageType::Response;
}

std::vector<uint8_t> ResponseMessage::encode() const {
    // Create the dictionary
    bencode::dict dict;
    
    // Add the transaction ID
    dict["t"] = m_transactionID;
    
    // Add the message type
    dict["y"] = "r";
    
    // Add the response values
    bencode::dict values = getResponseValues();
    
    // Add the node ID to the response values
    if (m_nodeID) {
        std::string nodeIDStr(reinterpret_cast<const char*>(m_nodeID->data()), m_nodeID->size());
        values["id"] = nodeIDStr;
    }
    
    dict["r"] = values;
    
    // Encode the dictionary
    std::string encoded = bencode::encode(dict);
    
    // Convert to byte vector
    return std::vector<uint8_t>(encoded.begin(), encoded.end());
}

std::shared_ptr<ResponseMessage> ResponseMessage::decode(const bencode::dict& dict) {
    auto logger = event::Logger::forComponent("DHT.ResponseMessage");
    
    // Check if the dictionary has response values
    auto rIt = dict.find("r");
    if (rIt == dict.end() || !std::holds_alternative<bencode::dict>(rIt->second)) {
        logger.error("Dictionary does not have valid response values");
        return nullptr;
    }
    
    // Get the response values
    const auto& responseValues = std::get<bencode::dict>(rIt->second);
    
    // Check if the response values have a node ID
    auto idIt = responseValues.find("id");
    if (idIt == responseValues.end() || !std::holds_alternative<bencode::string>(idIt->second)) {
        logger.error("Response values do not have a valid node ID");
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
    
    // Determine the response type based on the response values
    if (responseValues.find("nodes") != responseValues.end() && responseValues.find("token") != responseValues.end()) {
        // This is a get_peers response with nodes
        return GetPeersResponse::create(transactionID, nodeID, responseValues);
    } else if (responseValues.find("values") != responseValues.end() && responseValues.find("token") != responseValues.end()) {
        // This is a get_peers response with peers
        return GetPeersResponse::create(transactionID, nodeID, responseValues);
    } else if (responseValues.find("nodes") != responseValues.end()) {
        // This is a find_node response
        return FindNodeResponse::create(transactionID, nodeID, responseValues);
    } else {
        // This is either a ping response or an announce_peer response
        // Since they have the same format (just the node ID), we'll return a ping response
        return PingResponse::create(transactionID, nodeID, responseValues);
    }
}

PingResponse::PingResponse(const std::string& transactionID, const NodeID& nodeID)
    : ResponseMessage(transactionID, nodeID) {
}

std::shared_ptr<PingResponse> PingResponse::create(const std::string& transactionID, const NodeID& nodeID, const bencode::dict& responseValues) {
    return std::make_shared<PingResponse>(transactionID, nodeID);
}

bencode::dict PingResponse::getResponseValues() const {
    return {};
}

FindNodeResponse::FindNodeResponse(const std::string& transactionID, const NodeID& nodeID, const std::vector<std::shared_ptr<Node>>& nodes)
    : ResponseMessage(transactionID, nodeID), m_nodes(nodes) {
}

const std::vector<std::shared_ptr<Node>>& FindNodeResponse::getNodes() const {
    return m_nodes;
}

std::shared_ptr<FindNodeResponse> FindNodeResponse::create(const std::string& transactionID, const NodeID& nodeID, const bencode::dict& responseValues) {
    auto logger = event::Logger::forComponent("DHT.FindNodeResponse");
    
    // Check if the response values have nodes
    auto nodesIt = responseValues.find("nodes");
    if (nodesIt == responseValues.end() || !std::holds_alternative<bencode::string>(nodesIt->second)) {
        logger.error("Response values do not have valid nodes");
        return nullptr;
    }
    
    // Get the nodes
    const auto& nodesStr = std::get<bencode::string>(nodesIt->second);
    
    // Parse the nodes
    std::vector<std::shared_ptr<Node>> nodes;
    
    // Each node is 26 bytes: 20 bytes for the node ID, 4 bytes for the IP address, and 2 bytes for the port
    if (nodesStr.size() % 26 != 0) {
        logger.error("Nodes string has invalid size: {}", nodesStr.size());
        return nullptr;
    }
    
    for (size_t i = 0; i < nodesStr.size(); i += 26) {
        // Extract the node ID
        NodeID nodeID;
        std::copy(nodesStr.begin() + i, nodesStr.begin() + i + 20, nodeID.begin());
        
        // Extract the IP address
        std::stringstream ss;
        ss << static_cast<int>(static_cast<uint8_t>(nodesStr[i + 20])) << "."
           << static_cast<int>(static_cast<uint8_t>(nodesStr[i + 21])) << "."
           << static_cast<int>(static_cast<uint8_t>(nodesStr[i + 22])) << "."
           << static_cast<int>(static_cast<uint8_t>(nodesStr[i + 23]));
        std::string ipStr = ss.str();
        
        // Extract the port
        uint16_t port = (static_cast<uint16_t>(static_cast<uint8_t>(nodesStr[i + 24])) << 8) |
                         static_cast<uint16_t>(static_cast<uint8_t>(nodesStr[i + 25]));
        
        // Create the node
        network::NetworkAddress address(ipStr);
        network::EndPoint endpoint(address, port);
        auto node = std::make_shared<Node>(nodeID, endpoint);
        
        nodes.push_back(node);
    }
    
    return std::make_shared<FindNodeResponse>(transactionID, nodeID, nodes);
}

bencode::dict FindNodeResponse::getResponseValues() const {
    bencode::dict values;
    
    // Add the nodes
    std::string nodesStr;
    for (const auto& node : m_nodes) {
        // Add the node ID
        nodesStr.append(reinterpret_cast<const char*>(node->getID().data()), node->getID().size());
        
        // Add the IP address
        const auto& address = node->getEndpoint().getAddress();
        std::string ipStr = address.toString();
        std::vector<uint8_t> ipBytes = address.toBytes();
        nodesStr.append(reinterpret_cast<const char*>(ipBytes.data()), ipBytes.size());
        
        // Add the port
        uint16_t port = node->getEndpoint().getPort();
        nodesStr.push_back(static_cast<char>((port >> 8) & 0xFF));
        nodesStr.push_back(static_cast<char>(port & 0xFF));
    }
    
    values["nodes"] = nodesStr;
    
    return values;
}

GetPeersResponse::GetPeersResponse(const std::string& transactionID, const NodeID& nodeID, const std::vector<std::shared_ptr<Node>>& nodes, const std::string& token)
    : ResponseMessage(transactionID, nodeID), m_nodes(nodes), m_token(token), m_hasNodes(true), m_hasPeers(false) {
}

GetPeersResponse::GetPeersResponse(const std::string& transactionID, const NodeID& nodeID, const std::vector<network::EndPoint>& peers, const std::string& token)
    : ResponseMessage(transactionID, nodeID), m_peers(peers), m_token(token), m_hasNodes(false), m_hasPeers(true) {
}

const std::vector<std::shared_ptr<Node>>& GetPeersResponse::getNodes() const {
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

std::shared_ptr<GetPeersResponse> GetPeersResponse::create(const std::string& transactionID, const NodeID& nodeID, const bencode::dict& responseValues) {
    auto logger = event::Logger::forComponent("DHT.GetPeersResponse");
    
    // Check if the response values have a token
    auto tokenIt = responseValues.find("token");
    if (tokenIt == responseValues.end() || !std::holds_alternative<bencode::string>(tokenIt->second)) {
        logger.error("Response values do not have a valid token");
        return nullptr;
    }
    
    // Get the token
    std::string token = std::get<bencode::string>(tokenIt->second);
    
    // Check if the response values have nodes
    auto nodesIt = responseValues.find("nodes");
    if (nodesIt != responseValues.end() && std::holds_alternative<bencode::string>(nodesIt->second)) {
        // This is a get_peers response with nodes
        const auto& nodesStr = std::get<bencode::string>(nodesIt->second);
        
        // Parse the nodes
        std::vector<std::shared_ptr<Node>> nodes;
        
        // Each node is 26 bytes: 20 bytes for the node ID, 4 bytes for the IP address, and 2 bytes for the port
        if (nodesStr.size() % 26 != 0) {
            logger.error("Nodes string has invalid size: {}", nodesStr.size());
            return nullptr;
        }
        
        for (size_t i = 0; i < nodesStr.size(); i += 26) {
            // Extract the node ID
            NodeID nodeID;
            std::copy(nodesStr.begin() + i, nodesStr.begin() + i + 20, nodeID.begin());
            
            // Extract the IP address
            std::stringstream ss;
            ss << static_cast<int>(static_cast<uint8_t>(nodesStr[i + 20])) << "."
               << static_cast<int>(static_cast<uint8_t>(nodesStr[i + 21])) << "."
               << static_cast<int>(static_cast<uint8_t>(nodesStr[i + 22])) << "."
               << static_cast<int>(static_cast<uint8_t>(nodesStr[i + 23]));
            std::string ipStr = ss.str();
            
            // Extract the port
            uint16_t port = (static_cast<uint16_t>(static_cast<uint8_t>(nodesStr[i + 24])) << 8) |
                             static_cast<uint16_t>(static_cast<uint8_t>(nodesStr[i + 25]));
            
            // Create the node
            network::NetworkAddress address(ipStr);
            network::EndPoint endpoint(address, port);
            auto node = std::make_shared<Node>(nodeID, endpoint);
            
            nodes.push_back(node);
        }
        
        return std::make_shared<GetPeersResponse>(transactionID, nodeID, nodes, token);
    }
    
    // Check if the response values have peers
    auto valuesIt = responseValues.find("values");
    if (valuesIt != responseValues.end() && std::holds_alternative<bencode::list>(valuesIt->second)) {
        // This is a get_peers response with peers
        const auto& valuesList = std::get<bencode::list>(valuesIt->second);
        
        // Parse the peers
        std::vector<network::EndPoint> peers;
        
        for (const auto& value : valuesList) {
            if (!std::holds_alternative<bencode::string>(value)) {
                logger.error("Peer value is not a string");
                continue;
            }
            
            const auto& peerStr = std::get<bencode::string>(value);
            
            // Each peer is 6 bytes: 4 bytes for the IP address and 2 bytes for the port
            if (peerStr.size() != 6) {
                logger.error("Peer string has invalid size: {}", peerStr.size());
                continue;
            }
            
            // Extract the IP address
            std::stringstream ss;
            ss << static_cast<int>(static_cast<uint8_t>(peerStr[0])) << "."
               << static_cast<int>(static_cast<uint8_t>(peerStr[1])) << "."
               << static_cast<int>(static_cast<uint8_t>(peerStr[2])) << "."
               << static_cast<int>(static_cast<uint8_t>(peerStr[3]));
            std::string ipStr = ss.str();
            
            // Extract the port
            uint16_t port = (static_cast<uint16_t>(static_cast<uint8_t>(peerStr[4])) << 8) |
                             static_cast<uint16_t>(static_cast<uint8_t>(peerStr[5]));
            
            // Create the endpoint
            network::NetworkAddress address(ipStr);
            network::EndPoint endpoint(address, port);
            
            peers.push_back(endpoint);
        }
        
        return std::make_shared<GetPeersResponse>(transactionID, nodeID, peers, token);
    }
    
    logger.error("Response values do not have valid nodes or peers");
    return nullptr;
}

bencode::dict GetPeersResponse::getResponseValues() const {
    bencode::dict values;
    
    // Add the token
    values["token"] = m_token;
    
    if (m_hasNodes) {
        // Add the nodes
        std::string nodesStr;
        for (const auto& node : m_nodes) {
            // Add the node ID
            nodesStr.append(reinterpret_cast<const char*>(node->getID().data()), node->getID().size());
            
            // Add the IP address
            const auto& address = node->getEndpoint().getAddress();
            std::string ipStr = address.toString();
            std::vector<uint8_t> ipBytes = address.toBytes();
            nodesStr.append(reinterpret_cast<const char*>(ipBytes.data()), ipBytes.size());
            
            // Add the port
            uint16_t port = node->getEndpoint().getPort();
            nodesStr.push_back(static_cast<char>((port >> 8) & 0xFF));
            nodesStr.push_back(static_cast<char>(port & 0xFF));
        }
        
        values["nodes"] = nodesStr;
    } else if (m_hasPeers) {
        // Add the peers
        bencode::list peersList;
        for (const auto& peer : m_peers) {
            // Add the IP address
            const auto& address = peer.getAddress();
            std::vector<uint8_t> ipBytes = address.toBytes();
            
            // Add the port
            uint16_t port = peer.getPort();
            
            // Create the peer string
            std::string peerStr;
            peerStr.append(reinterpret_cast<const char*>(ipBytes.data()), ipBytes.size());
            peerStr.push_back(static_cast<char>((port >> 8) & 0xFF));
            peerStr.push_back(static_cast<char>(port & 0xFF));
            
            peersList.push_back(peerStr);
        }
        
        values["values"] = peersList;
    }
    
    return values;
}

AnnouncePeerResponse::AnnouncePeerResponse(const std::string& transactionID, const NodeID& nodeID)
    : ResponseMessage(transactionID, nodeID) {
}

std::shared_ptr<AnnouncePeerResponse> AnnouncePeerResponse::create(const std::string& transactionID, const NodeID& nodeID, const bencode::dict& responseValues) {
    return std::make_shared<AnnouncePeerResponse>(transactionID, nodeID);
}

bencode::dict AnnouncePeerResponse::getResponseValues() const {
    return {};
}

} // namespace dht_hunter::dht
