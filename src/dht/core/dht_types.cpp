#include "dht_hunter/dht/core/dht_types.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace dht_hunter::dht {

NodeID calculateDistance(const NodeID& a, const NodeID& b) {
    NodeID distance;
    for (size_t i = 0; i < a.size(); ++i) {
        distance[i] = a[i] ^ b[i];
    }
    return distance;
}

std::string nodeIDToString(const NodeID& nodeID) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (const auto& byte : nodeID) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

std::optional<NodeID> stringToNodeID(const std::string& str) {
    // Check if the string is a valid hex string of the right length
    if (str.length() != 40) {
        return std::nullopt;
    }

    for (char c : str) {
        if (!std::isxdigit(c)) {
            return std::nullopt;
        }
    }

    NodeID nodeID;
    for (size_t i = 0; i < 20; ++i) {
        std::string byteStr = str.substr(i * 2, 2);
        nodeID[i] = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
    }

    return nodeID;
}

std::string infoHashToString(const InfoHash& infoHash) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (const auto& byte : infoHash) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

std::optional<InfoHash> stringToInfoHash(const std::string& str) {
    // Check if the string is a valid hex string of the right length
    if (str.length() != 40) {
        return std::nullopt;
    }

    for (char c : str) {
        if (!std::isxdigit(c)) {
            return std::nullopt;
        }
    }

    InfoHash infoHash;
    for (size_t i = 0; i < 20; ++i) {
        std::string byteStr = str.substr(i * 2, 2);
        infoHash[i] = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
    }

    return infoHash;
}

NodeID generateRandomNodeID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint8_t> dist(0, 255);

    NodeID nodeID;
    for (auto& byte : nodeID) {
        byte = dist(gen);
    }

    return nodeID;
}

bool isValidNodeID(const NodeID& nodeID) {
    // Check if the node ID is all zeros
    return !std::all_of(nodeID.begin(), nodeID.end(), [](uint8_t byte) { return byte == 0; });
}

bool isValidInfoHash(const InfoHash& infoHash) {
    // Check if the info hash is all zeros
    return !std::all_of(infoHash.begin(), infoHash.end(), [](uint8_t byte) { return byte == 0; });
}

Node::Node(const NodeID& id, const network::EndPoint& endpoint)
    : m_id(id), m_endpoint(endpoint) {
}

const NodeID& Node::getID() const {
    return m_id;
}

const network::EndPoint& Node::getEndpoint() const {
    return m_endpoint;
}

void Node::setEndpoint(const network::EndPoint& endpoint) {
    m_endpoint = endpoint;
}

} // namespace dht_hunter::dht
