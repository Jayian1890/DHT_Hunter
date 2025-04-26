#include "dht_hunter/dht/core/dht_types.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/event/logger.hpp"

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

std::string infoHashToString(const InfoHash& infoHash) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (const auto& byte : infoHash) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

NodeID generateRandomNodeID() {
    NodeID nodeID;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (size_t i = 0; i < nodeID.size(); ++i) {
        nodeID[i] = static_cast<uint8_t>(dis(gen));
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
    : m_id(id), m_endpoint(endpoint), m_lastSeen(std::chrono::steady_clock::now()) {
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

std::chrono::steady_clock::time_point Node::getLastSeen() const {
    return m_lastSeen;
}

void Node::updateLastSeen() {
    m_lastSeen = std::chrono::steady_clock::now();
}

} // namespace dht_hunter::dht
