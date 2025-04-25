#include "dht_hunter/dht/network/dht_response_message.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"

DEFINE_COMPONENT_LOGGER("DHT", "ResponseMessage")

namespace dht_hunter::dht {

ResponseMessage::ResponseMessage(const std::string& transactionID, const NodeID& nodeID)
    : DHTMessage(transactionID), m_nodeID(nodeID) {
}

ResponseMessage::ResponseMessage(const std::string& transactionID)
    : DHTMessage(transactionID), m_nodeID() {
}

MessageType ResponseMessage::getType() const {
    return MessageType::Response;
}

const NodeID& ResponseMessage::getNodeIDRef() const {
    return m_nodeID;
}

std::optional<NodeID> ResponseMessage::getNodeID() const {
    return m_nodeID;
}

void ResponseMessage::setNodeID(const NodeID& nodeID) {
    m_nodeID = nodeID;
}

bencode::BencodeDict ResponseMessage::encode() const {
    bencode::BencodeDict dict;

    // Add the transaction ID
    auto tValue = std::make_shared<bencode::BencodeValue>();
    tValue->setString(m_transactionID);
    dict["t"] = tValue;

    // Add the message type
    auto yValue = std::make_shared<bencode::BencodeValue>();
    yValue->setString("r");
    dict["y"] = yValue;

    // Add the response values
    bencode::BencodeDict rDict;

    // Add the node ID
    auto idValue = std::make_shared<bencode::BencodeValue>();
    idValue->setString(nodeIDToString(m_nodeID));
    rDict["id"] = idValue;

    // Add the response-specific values
    encodeValues(rDict);

    // Add the response values to the dictionary
    auto rValue = std::make_shared<bencode::BencodeValue>();
    rValue->setDict(rDict);
    dict["r"] = rValue;

    return dict;
}

std::shared_ptr<ResponseMessage> ResponseMessage::decode(const bencode::BencodeDict& dict, QueryMethod queryMethod) {
    // Check if the dictionary has a transaction ID
    auto transactionIDIt = dict.find("t");
    if (transactionIDIt == dict.end() || !transactionIDIt->second->isString()) {
        getLogger()->error("Response message does not have a valid transaction ID");
        return nullptr;
    }
    std::string transactionID = transactionIDIt->second->getString();

    // Check if the dictionary has response values
    auto rIt = dict.find("r");
    if (rIt == dict.end() || !rIt->second->isDictionary()) {
        getLogger()->error("Response message does not have valid response values");
        return nullptr;
    }
    const auto& rDict = rIt->second->getDictionary();

    // Check if the response values have a node ID
    auto nodeIDIt = rDict.find("id");
    if (nodeIDIt == rDict.end() || !nodeIDIt->second->isString()) {
        getLogger()->error("Response message values do not have a valid node ID");
        return nullptr;
    }
    auto nodeIDOpt = stringToNodeID(nodeIDIt->second->getString());
    if (!nodeIDOpt) {
        getLogger()->error("Response message values have an invalid node ID");
        return nullptr;
    }
    NodeID nodeID = nodeIDOpt.value();

    // Create the response message based on the query method
    std::shared_ptr<ResponseMessage> response;
    switch (queryMethod) {
        case QueryMethod::Ping:
            response = std::make_shared<PingResponse>(transactionID, nodeID);
            break;
        case QueryMethod::FindNode:
            response = std::make_shared<FindNodeResponse>(transactionID, nodeID);
            break;
        case QueryMethod::GetPeers:
            response = std::make_shared<GetPeersResponse>(transactionID, nodeID);
            break;
        case QueryMethod::AnnouncePeer:
            response = std::make_shared<AnnouncePeerResponse>(transactionID, nodeID);
            break;
        default:
            getLogger()->error("Unknown query method: {}", static_cast<int>(queryMethod));
            return nullptr;
    }

    // Decode the response-specific values
    if (!response->decodeValues(rDict)) {
        getLogger()->error("Failed to decode response-specific values");
        return nullptr;
    }

    return response;
}

PingResponse::PingResponse(const std::string& transactionID, const NodeID& nodeID)
    : ResponseMessage(transactionID, nodeID) {
}

void PingResponse::encodeValues(bencode::BencodeDict& /*dict*/) const {
    // No additional values for ping response
}

bool PingResponse::decodeValues(const bencode::BencodeDict& /*dict*/) {
    // No additional values for ping response
    return true;
}

FindNodeResponse::FindNodeResponse(const std::string& transactionID, const NodeID& nodeID)
    : ResponseMessage(transactionID, nodeID) {
}

const std::vector<std::shared_ptr<Node>>& FindNodeResponse::getNodes() const {
    return m_nodes;
}

void FindNodeResponse::addNode(std::shared_ptr<Node> node) {
    m_nodes.push_back(node);
}

void FindNodeResponse::setNodes(const std::vector<std::shared_ptr<Node>>& nodes) {
    m_nodes = nodes;
}

void FindNodeResponse::encodeValues(bencode::BencodeDict& dict) const {
    // Add the nodes
    std::string nodesStr;
    for (const auto& node : m_nodes) {
        // Add the node ID
        nodesStr += nodeIDToString(node->getID());

        // Add the node IP
        const auto& address = node->getEndpoint().getAddress();
        if (address.isIPv4()) {
            // IPv4 address
            nodesStr += address.toBytes();
        } else {
            // IPv6 address (not supported by the DHT protocol)
            getLogger()->warning("IPv6 address not supported by the DHT protocol");
            continue;
        }

        // Add the node port
        uint16_t port = node->getEndpoint().getPort();
        nodesStr += static_cast<char>((port >> 8) & 0xFF);
        nodesStr += static_cast<char>(port & 0xFF);
    }

    // Add the nodes string to the dictionary
    auto nodesValue = std::make_shared<bencode::BencodeValue>();
    nodesValue->setString(nodesStr);
    dict["nodes"] = nodesValue;
}

bool FindNodeResponse::decodeValues(const bencode::BencodeDict& dict) {
    // Check if the response values have nodes
    auto nodesIt = dict.find("nodes");
    if (nodesIt == dict.end() || !nodesIt->second->isString()) {
        getLogger()->error("Find_node response values do not have valid nodes");
        return false;
    }
    std::string nodesStr = nodesIt->second->getString();

    // Parse the nodes
    m_nodes.clear();
    for (size_t i = 0; i + 26 <= nodesStr.size(); i += 26) {
        // Parse the node ID
        NodeID nodeID;
        std::copy(nodesStr.begin() + static_cast<std::ptrdiff_t>(i), nodesStr.begin() + static_cast<std::ptrdiff_t>(i + 20), nodeID.begin());

        // Parse the node IP
        std::string ipStr;
        for (size_t j = 0; j < 4; ++j) {
            if (j > 0) {
                ipStr += ".";
            }
            ipStr += std::to_string(static_cast<uint8_t>(nodesStr[i + 20 + j]));
        }

        // Parse the node port
        uint8_t high_byte = static_cast<uint8_t>(nodesStr[i + 24]);
        uint8_t low_byte = static_cast<uint8_t>(nodesStr[i + 25]);
        uint16_t high = static_cast<uint16_t>(high_byte);
        uint16_t low = static_cast<uint16_t>(low_byte);
        uint16_t port = static_cast<uint16_t>((static_cast<unsigned int>(high) << 8U) | static_cast<unsigned int>(low));

        // Create the node
        network::NetworkAddress address(ipStr);
        network::EndPoint endpoint(address, port);
        auto node = std::make_shared<Node>(nodeID, endpoint);

        // Add the node to the list
        m_nodes.push_back(node);
    }

    return true;
}

GetPeersResponse::GetPeersResponse(const std::string& transactionID, const NodeID& nodeID)
    : ResponseMessage(transactionID, nodeID) {
}

std::optional<std::string> GetPeersResponse::getToken() const {
    if (m_token.empty()) {
        return std::nullopt;
    }
    return m_token;
}

void GetPeersResponse::setToken(const std::string& token) {
    m_token = token;
}

const std::vector<std::shared_ptr<Node>>& GetPeersResponse::getNodes() const {
    return m_nodes;
}

void GetPeersResponse::addNode(std::shared_ptr<Node> node) {
    m_nodes.push_back(node);
}

void GetPeersResponse::setNodes(const std::vector<std::shared_ptr<Node>>& nodes) {
    m_nodes = nodes;
}

std::optional<std::vector<network::EndPoint>> GetPeersResponse::getPeers() const {
    if (m_peers.empty()) {
        return std::nullopt;
    }
    return m_peers;
}

void GetPeersResponse::addPeer(const network::EndPoint& peer) {
    m_peers.push_back(peer);
}

void GetPeersResponse::setPeers(const std::vector<network::EndPoint>& peers) {
    m_peers = peers;
}

void GetPeersResponse::encodeValues(bencode::BencodeDict& dict) const {
    // Add the token
    auto tokenValue = std::make_shared<bencode::BencodeValue>();
    tokenValue->setString(m_token);
    dict["token"] = tokenValue;

    // Add either nodes or peers
    if (!m_peers.empty()) {
        // Add the peers
        bencode::BencodeList peersList;
        for (const auto& peer : m_peers) {
            // Add the peer as a string
            std::string peerStr;

            // Add the peer IP
            const auto& address = peer.getAddress();
            if (address.isIPv4()) {
                // IPv4 address
                peerStr += address.toBytes();
            } else {
                // IPv6 address (not supported by the DHT protocol)
                getLogger()->warning("IPv6 address not supported by the DHT protocol");
                continue;
            }

            // Add the peer port
            uint16_t port = peer.getPort();
            peerStr += static_cast<char>((port >> 8) & 0xFF);
            peerStr += static_cast<char>(port & 0xFF);

            // Add the peer string to the list
            auto peerValue = std::make_shared<bencode::BencodeValue>();
            peerValue->setString(peerStr);
            peersList.push_back(peerValue);
        }

        // Add the peers list to the dictionary
        auto peersValue = std::make_shared<bencode::BencodeValue>();
        peersValue->setList(peersList);
        dict["values"] = peersValue;
    } else {
        // Add the nodes
        std::string nodesStr;
        for (const auto& node : m_nodes) {
            // Add the node ID
            nodesStr += nodeIDToString(node->getID());

            // Add the node IP
            const auto& address = node->getEndpoint().getAddress();
            if (address.isIPv4()) {
                // IPv4 address
                nodesStr += address.toBytes();
            } else {
                // IPv6 address (not supported by the DHT protocol)
                getLogger()->warning("IPv6 address not supported by the DHT protocol");
                continue;
            }

            // Add the node port
            uint16_t port = node->getEndpoint().getPort();
            nodesStr += static_cast<char>((port >> 8) & 0xFF);
            nodesStr += static_cast<char>(port & 0xFF);
        }

        // Add the nodes string to the dictionary
        auto nodesValue = std::make_shared<bencode::BencodeValue>();
        nodesValue->setString(nodesStr);
        dict["nodes"] = nodesValue;
    }
}

bool GetPeersResponse::decodeValues(const bencode::BencodeDict& dict) {
    // Check if the response values have a token
    auto tokenIt = dict.find("token");
    if (tokenIt == dict.end() || !tokenIt->second->isString()) {
        getLogger()->error("Get_peers response values do not have a valid token");
        return false;
    }
    m_token = tokenIt->second->getString();

    // Check if the response values have peers
    auto valuesIt = dict.find("values");
    if (valuesIt != dict.end() && valuesIt->second->isList()) {
        // Parse the peers
        m_peers.clear();
        const auto& peersList = valuesIt->second->getList();
        for (const auto& peerValue : peersList) {
            if (!peerValue->isString()) {
                getLogger()->warning("Peer value is not a string");
                continue;
            }
            std::string peerStr = peerValue->getString();
            if (peerStr.size() != 6) {
                getLogger()->warning("Peer string has invalid size: {}", peerStr.size());
                continue;
            }

            // Parse the peer IP
            std::string ipStr;
            for (size_t i = 0; i < 4; ++i) {
                if (i > 0) {
                    ipStr += ".";
                }
                ipStr += std::to_string(static_cast<uint8_t>(peerStr[i]));
            }

            // Parse the peer port
            uint8_t high_byte = static_cast<uint8_t>(peerStr[4]);
            uint8_t low_byte = static_cast<uint8_t>(peerStr[5]);
            uint16_t high = static_cast<uint16_t>(high_byte);
            uint16_t low = static_cast<uint16_t>(low_byte);
            uint16_t port = static_cast<uint16_t>((static_cast<unsigned int>(high) << 8U) | static_cast<unsigned int>(low));

            // Create the peer
            network::NetworkAddress address(ipStr);
            network::EndPoint endpoint(address, port);

            // Add the peer to the list
            m_peers.push_back(endpoint);
        }
    } else {
        // Check if the response values have nodes
        auto nodesIt = dict.find("nodes");
        if (nodesIt == dict.end() || !nodesIt->second->isString()) {
            getLogger()->error("Get_peers response values do not have valid nodes or peers");
            return false;
        }
        std::string nodesStr = nodesIt->second->getString();

        // Parse the nodes
        m_nodes.clear();
        for (size_t i = 0; i + 26 <= nodesStr.size(); i += 26) {
            // Parse the node ID
            NodeID nodeID;
            std::copy(nodesStr.begin() + static_cast<std::ptrdiff_t>(i), nodesStr.begin() + static_cast<std::ptrdiff_t>(i + 20), nodeID.begin());

            // Parse the node IP
            std::string ipStr;
            for (size_t j = 0; j < 4; ++j) {
                if (j > 0) {
                    ipStr += ".";
                }
                ipStr += std::to_string(static_cast<uint8_t>(nodesStr[i + 20 + j]));
            }

            // Parse the node port
            uint8_t high_byte = static_cast<uint8_t>(nodesStr[i + 24]);
            uint8_t low_byte = static_cast<uint8_t>(nodesStr[i + 25]);
            uint16_t high = static_cast<uint16_t>(high_byte);
            uint16_t low = static_cast<uint16_t>(low_byte);
            uint16_t port = static_cast<uint16_t>((static_cast<unsigned int>(high) << 8U) | static_cast<unsigned int>(low));

            // Create the node
            network::NetworkAddress address(ipStr);
            network::EndPoint endpoint(address, port);
            auto node = std::make_shared<Node>(nodeID, endpoint);

            // Add the node to the list
            m_nodes.push_back(node);
        }
    }

    return true;
}

AnnouncePeerResponse::AnnouncePeerResponse(const std::string& transactionID, const NodeID& nodeID)
    : ResponseMessage(transactionID, nodeID) {
}

void AnnouncePeerResponse::encodeValues(bencode::BencodeDict& /*dict*/) const {
    // No additional values for announce_peer response
}

bool AnnouncePeerResponse::decodeValues(const bencode::BencodeDict& /*dict*/) {
    // No additional values for announce_peer response
    return true;
}

} // namespace dht_hunter::dht
