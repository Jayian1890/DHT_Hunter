#include "dht_hunter/dht/network/response_message.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"
#include <sstream>

namespace dht_hunter::dht {

ResponseMessage::ResponseMessage(const std::string& transactionID, const NodeID& nodeID)
    : Message(transactionID, nodeID) {
}

const std::string& ResponseMessage::getSenderIP() const {
    return m_senderIP;
}

void ResponseMessage::setSenderIP(const std::string& senderIP) {
    m_senderIP = senderIP;
}

MessageType ResponseMessage::getType() const {
    return MessageType::Response;
}

std::vector<uint8_t> ResponseMessage::encode() const {
    // Create the dictionary
    auto dict = std::make_shared<dht_hunter::bencode::BencodeValue>();
    dht_hunter::bencode::BencodeValue::Dictionary emptyDict;
    dict->setDict(emptyDict);

    // Add the transaction ID
    dict->setString("t", m_transactionID);

    // Add the message type
    dict->setString("y", "r");

    // Get the response values
    auto values = getResponseValues();

    // Add the node ID to the response values
    if (m_nodeID) {
        std::string nodeIDStr(reinterpret_cast<const char*>(m_nodeID->data()), m_nodeID->size());
        values->setString("id", nodeIDStr);
    }

    // Add the response values to the dictionary
    dict->set("r", values);

    // Add the client version if available
    if (!m_clientVersion.empty()) {
        dict->setString("v", m_clientVersion);
    }

    // Add the sender IP if available
    if (!m_senderIP.empty()) {
        dict->setString("ip", m_senderIP);
    }

    // Encode the dictionary
    std::string encoded = dht_hunter::bencode::BencodeEncoder::encode(dict);

    // Convert to byte vector
    return std::vector<uint8_t>(encoded.begin(), encoded.end());
}

std::shared_ptr<ResponseMessage> ResponseMessage::decode(const dht_hunter::bencode::BencodeValue& value) {    // Logger initialization removed

    // Check if the value is a dictionary
    if (!value.isDictionary()) {
        return nullptr;
    }

    // Check if the dictionary has response values
    auto responseValues = value.getDict("r");
    if (!responseValues) {
        return nullptr;
    }

    // Create a BencodeValue from the Dictionary
    dht_hunter::bencode::BencodeValue responseValuesObj(*responseValues);

    // Check if the response values have a node ID
    auto nodeIDStr = responseValuesObj.getString("id");
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

    // Check for sender IP (optional)
    std::string senderIP;
    auto ipValue = value.getString("ip");
    if (ipValue) {
        senderIP = *ipValue;
    }

    // Determine the response type based on the response values
    std::shared_ptr<ResponseMessage> response;
    if (responseValuesObj.contains("nodes") && responseValuesObj.contains("token")) {
        // This is a get_peers response with nodes
        response = GetPeersResponse::create(*transactionID, nodeID, responseValuesObj);
    } else if (responseValuesObj.contains("values") && responseValuesObj.contains("token")) {
        // This is a get_peers response with peers
        response = GetPeersResponse::create(*transactionID, nodeID, responseValuesObj);
    } else if (responseValuesObj.contains("nodes")) {
        // This is a find_node response
        response = FindNodeResponse::create(*transactionID, nodeID, responseValuesObj);
    } else {
        // This is either a ping response or an announce_peer response
        // Since they have the same format (just the node ID), we'll return a ping response
        response = PingResponse::create(*transactionID, nodeID, responseValuesObj);
    }

    // Set the sender IP if available
    if (response && !senderIP.empty()) {
        response->setSenderIP(senderIP);
    }

    return response;
}

PingResponse::PingResponse(const std::string& transactionID, const NodeID& nodeID)
    : ResponseMessage(transactionID, nodeID) {
}

std::shared_ptr<PingResponse> PingResponse::create(const std::string& transactionID, const NodeID& nodeID, const dht_hunter::bencode::BencodeValue& /*responseValues*/) {
    return std::make_shared<PingResponse>(transactionID, nodeID);
}

std::shared_ptr<dht_hunter::bencode::BencodeValue> PingResponse::getResponseValues() const {
    auto values = std::make_shared<dht_hunter::bencode::BencodeValue>();
    dht_hunter::bencode::BencodeValue::Dictionary emptyDict;
    values->setDict(emptyDict);
    return values;
}

FindNodeResponse::FindNodeResponse(const std::string& transactionID, const NodeID& nodeID, const std::vector<std::shared_ptr<Node>>& nodes)
    : ResponseMessage(transactionID, nodeID), m_nodes(nodes) {
}

const std::vector<std::shared_ptr<Node>>& FindNodeResponse::getNodes() const {
    return m_nodes;
}

std::shared_ptr<FindNodeResponse> FindNodeResponse::create(const std::string& transactionID, const NodeID& nodeID, const dht_hunter::bencode::BencodeValue& responseValues) {    // Logger initialization removed

    // Check if the response values have nodes
    auto nodesStr = responseValues.getString("nodes");
    if (!nodesStr) {
        return nullptr;
    }

    // Parse the nodes
    std::vector<std::shared_ptr<Node>> nodes;

    // Each node is 26 bytes: 20 bytes for the node ID, 4 bytes for the IP address, and 2 bytes for the port
    if (nodesStr->size() % 26 != 0) {
        return nullptr;
    }

    for (size_t i = 0; i < nodesStr->size(); i += 26) {
        // Extract the node ID
        NodeID nodeId;
        std::copy(nodesStr->begin() + static_cast<long>(i), nodesStr->begin() + static_cast<long>(i + 20), nodeId.begin());

        // Extract the IP address
        std::stringstream ss;
        ss << static_cast<int>(static_cast<uint8_t>((*nodesStr)[i + 20])) << "."
           << static_cast<int>(static_cast<uint8_t>((*nodesStr)[i + 21])) << "."
           << static_cast<int>(static_cast<uint8_t>((*nodesStr)[i + 22])) << "."
           << static_cast<int>(static_cast<uint8_t>((*nodesStr)[i + 23]));
        std::string ipStr = ss.str();

        // Extract the port
        uint16_t port = static_cast<uint16_t>((static_cast<uint16_t>(static_cast<uint8_t>((*nodesStr)[i + 24])) << 8) |
                         static_cast<uint16_t>(static_cast<uint8_t>((*nodesStr)[i + 25])));

        // Create the node
        network::NetworkAddress address(ipStr);
        network::EndPoint endpoint(address, port);
        auto node = std::make_shared<Node>(nodeId, endpoint);

        nodes.push_back(node);
    }

    return std::make_shared<FindNodeResponse>(transactionID, nodeID, nodes);
}

std::shared_ptr<dht_hunter::bencode::BencodeValue> FindNodeResponse::getResponseValues() const {
    auto values = std::make_shared<dht_hunter::bencode::BencodeValue>();
    dht_hunter::bencode::BencodeValue::Dictionary emptyDict;
    values->setDict(emptyDict);

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

    values->setString("nodes", nodesStr);

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

std::shared_ptr<GetPeersResponse> GetPeersResponse::create(const std::string& transactionID, const NodeID& nodeID, const dht_hunter::bencode::BencodeValue& responseValues) {    // Logger initialization removed

    // Check if the response values have a token
    auto token = responseValues.getString("token");
    if (!token) {
        return nullptr;
    }

    // Check if the response values have nodes
    auto nodesStr = responseValues.getString("nodes");
    if (nodesStr) {
        // This is a get_peers response with nodes

        // Parse the nodes
        std::vector<std::shared_ptr<Node>> nodes;

        // Each node is 26 bytes: 20 bytes for the node ID, 4 bytes for the IP address, and 2 bytes for the port
        if (nodesStr->size() % 26 != 0) {
            return nullptr;
        }

        for (size_t i = 0; i < nodesStr->size(); i += 26) {
            // Extract the node ID
            NodeID nodeId;
            std::copy(nodesStr->begin() + static_cast<long>(i), nodesStr->begin() + static_cast<long>(i + 20), nodeId.begin());

            // Extract the IP address
            std::stringstream ss;
            ss << static_cast<int>(static_cast<uint8_t>((*nodesStr)[i + 20])) << "."
               << static_cast<int>(static_cast<uint8_t>((*nodesStr)[i + 21])) << "."
               << static_cast<int>(static_cast<uint8_t>((*nodesStr)[i + 22])) << "."
               << static_cast<int>(static_cast<uint8_t>((*nodesStr)[i + 23]));
            std::string ipStr = ss.str();

            // Extract the port
            uint16_t port = static_cast<uint16_t>((static_cast<uint16_t>(static_cast<uint8_t>((*nodesStr)[i + 24])) << 8) |
                             static_cast<uint16_t>(static_cast<uint8_t>((*nodesStr)[i + 25])));

            // Create the node
            network::NetworkAddress address(ipStr);
            network::EndPoint endpoint(address, port);
            auto node = std::make_shared<Node>(nodeId, endpoint);

            nodes.push_back(node);
        }

        return std::make_shared<GetPeersResponse>(transactionID, nodeID, nodes, *token);
    }

    // Check if the response values have peers
    auto valuesList = responseValues.getList("values");
    if (valuesList) {
        // This is a get_peers response with peers

        // Parse the peers
        std::vector<network::EndPoint> peers;

        for (size_t i = 0; i < valuesList->size(); ++i) {
            auto value = valuesList->at(i);
            if (!value->isString()) {
                continue;
            }

            const auto& peerStr = value->getString();

            // Each peer is 6 bytes: 4 bytes for the IP address and 2 bytes for the port
            if (peerStr.size() != 6) {
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
            uint16_t port = static_cast<uint16_t>((static_cast<uint16_t>(static_cast<uint8_t>(peerStr[4])) << 8) |
                             static_cast<uint16_t>(static_cast<uint8_t>(peerStr[5])));

            // Create the endpoint
            network::NetworkAddress address(ipStr);
            network::EndPoint endpoint(address, port);

            peers.push_back(endpoint);
        }

        return std::make_shared<GetPeersResponse>(transactionID, nodeID, peers, *token);
    }

    // If we get here, we have a token but no nodes or values
    // Return a response with empty nodes and peers
    return std::make_shared<GetPeersResponse>(transactionID, nodeID, std::vector<std::shared_ptr<Node>>(), *token);
}

std::shared_ptr<dht_hunter::bencode::BencodeValue> GetPeersResponse::getResponseValues() const {
    auto values = std::make_shared<dht_hunter::bencode::BencodeValue>();
    dht_hunter::bencode::BencodeValue::Dictionary emptyDict;
    values->setDict(emptyDict);

    // Add the token
    values->setString("token", m_token);

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

        values->setString("nodes", nodesStr);
    } else if (m_hasPeers) {
        // Add the peers
        auto peersList = std::make_shared<dht_hunter::bencode::BencodeValue>();
        peersList->setList(dht_hunter::bencode::BencodeValue::List());

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

            peersList->addString(peerStr);
        }

        values->setList("values", peersList->getList());
    }

    return values;
}

AnnouncePeerResponse::AnnouncePeerResponse(const std::string& transactionID, const NodeID& nodeID)
    : ResponseMessage(transactionID, nodeID) {
}

std::shared_ptr<AnnouncePeerResponse> AnnouncePeerResponse::create(const std::string& transactionID, const NodeID& nodeID, const dht_hunter::bencode::BencodeValue& /*responseValues*/) {
    return std::make_shared<AnnouncePeerResponse>(transactionID, nodeID);
}

std::shared_ptr<dht_hunter::bencode::BencodeValue> AnnouncePeerResponse::getResponseValues() const {
    auto values = std::make_shared<dht_hunter::bencode::BencodeValue>();
    dht_hunter::bencode::BencodeValue::Dictionary emptyDict;
    values->setDict(emptyDict);
    return values;
}

} // namespace dht_hunter::dht
