#include "dht_hunter/dht/types/dht_types.hpp"
#include "dht_hunter/dht/types/node_id.hpp"
#include "dht_hunter/logforge/logforge.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <random>
#include <sstream>

namespace dht_hunter::dht {

std::string nodeIDToString(const NodeID& nodeID) {
    return nodeID.toString();
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
    return NodeID::random();
}

InfoHash createEmptyInfoHash() {
    InfoHash emptyHash{}; // Initialize with all zeros
    return emptyHash;
}

bool isValidNodeID(const NodeID& nodeID) {
    return nodeID.isValid();
}

bool isValidInfoHash(const InfoHash& infoHash) {
    // Check if the info hash is all zeros
    return !std::ranges::all_of(infoHash, [](const uint8_t byte) { return byte == 0; });
}

NodeID calculateDistance(const NodeID& a, const NodeID& b) {
    return a.distanceTo(b);
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
