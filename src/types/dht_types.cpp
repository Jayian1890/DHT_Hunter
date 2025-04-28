#include "dht_hunter/types/dht_types.hpp"
#include <algorithm>

namespace dht_hunter::types {

NodeID calculateDistance(const NodeID& a, const NodeID& b) {
    return a.distanceTo(b);
}

std::string nodeIDToString(const NodeID& nodeID) {
    return nodeID.toString();
}

NodeID generateRandomNodeID() {
    return NodeID::random();
}

bool isValidNodeID(const NodeID& nodeID) {
    return nodeID.isValid();
}

Node::Node(const NodeID& id, const EndPoint& endpoint)
    : m_id(id), m_endpoint(endpoint), m_lastSeen(std::chrono::steady_clock::now()) {
}

const NodeID& Node::getID() const {
    return m_id;
}

const EndPoint& Node::getEndpoint() const {
    return m_endpoint;
}

void Node::setEndpoint(const EndPoint& endpoint) {
    m_endpoint = endpoint;
}

std::chrono::steady_clock::time_point Node::getLastSeen() const {
    return m_lastSeen;
}

void Node::updateLastSeen() {
    m_lastSeen = std::chrono::steady_clock::now();
}

} // namespace dht_hunter::types
