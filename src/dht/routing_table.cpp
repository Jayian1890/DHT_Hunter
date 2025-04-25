#include "dht_hunter/dht/routing_table.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include "dht_hunter/util/mutex_utils.hpp"
#include <algorithm>
#include <cassert>

DEFINE_COMPONENT_LOGGER("DHT", "RoutingTable")

namespace dht_hunter::dht {
// Node implementation
Node::Node(const NodeID& id, const network::EndPoint& endpoint)
    : m_id(id), m_endpoint(endpoint), m_lastSeen(std::chrono::steady_clock::now()),
      m_isGood(true), m_failedQueries(0) {
}
const NodeID& Node::getID() const {
    return m_id;
}
const network::EndPoint& Node::getEndpoint() const {
    return m_endpoint;
}
std::chrono::steady_clock::time_point Node::getLastSeen() const {
    return m_lastSeen;
}
void Node::updateLastSeen() {
    m_lastSeen = std::chrono::steady_clock::now();
}
bool Node::isGood() const {
    return m_isGood;
}
void Node::setGood(bool good) {
    m_isGood = good;
}
int Node::getFailedQueries() const {
    return m_failedQueries;
}
int Node::incrementFailedQueries() {
    return ++m_failedQueries;
}
void Node::resetFailedQueries() {
    m_failedQueries = 0;
}
// Distance calculation
NodeID calculateDistance(const NodeID& id1, const NodeID& id2) {
    NodeID result;
    for (size_t i = 0; i < NODE_ID_BITS / 8; i++) {
        result[i] = id1[i] ^ id2[i];
    }
    return result;
}
int getBucketIndex(const NodeID& distance) {
    // Find the index of the first bit that is 1
    for (size_t i = 0; i < NODE_ID_BITS / 8; i++) {
        if (distance[i] == 0) {
            continue;
        }
        // Found a byte with a 1 bit, find which bit
        uint8_t byte = distance[i];
        for (int j = 7; j >= 0; j--) {
            if ((byte & (1 << j)) != 0) {
                return static_cast<int>(i * 8) + (7 - j);
            }
        }
    }
    // All bits are 0, this is our own ID
    return -1;
}
// KBucket implementation
KBucket::KBucket(int index, size_t maxSize)
    : m_index(index), m_maxSize(maxSize) {
}
int KBucket::getIndex() const {
    return m_index;
}
const std::list<std::shared_ptr<Node>>& KBucket::getNodes() const {
    return m_nodes;
}
size_t KBucket::size() const {
    return m_nodes.size();
}
bool KBucket::isFull() const {
    return m_nodes.size() >= m_maxSize;
}
bool KBucket::addNode(std::shared_ptr<Node> node) {
    auto logger = dht_hunter::logforge::LogForge::getLogger("DHT.KBucket");

    // Check if the node is already in the bucket
    for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if ((*it)->getID() == node->getID()) {
            // Node already exists, move it to the end (most recently seen)
            logger->debug("Node already exists in bucket {}, moving to end: {}",
                      m_index, nodeIDToString(node->getID()));
            m_nodes.erase(it);
            m_nodes.push_back(node);
            return true;
        }
    }
    // If the bucket is full, we can't add the node
    if (isFull()) {
        logger->debug("Cannot add node to bucket {} because it's full: {}",
                  m_index, nodeIDToString(node->getID()));
        return false;
    }
    // Add the node to the end of the list (most recently seen)
    logger->debug("Adding node to bucket {}: {}", m_index, nodeIDToString(node->getID()));
    m_nodes.push_back(node);
    return true;
}
bool KBucket::removeNode(std::shared_ptr<Node> node) {
    for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if ((*it)->getID() == node->getID()) {
            m_nodes.erase(it);
            return true;
        }
    }
    return false;
}
std::shared_ptr<Node> KBucket::findNode(const NodeID& id) const {
    for (const auto& node : m_nodes) {
        if (node->getID() == id) {
            return node;
        }
    }
    return nullptr;
}
std::shared_ptr<Node> KBucket::getLeastRecentlySeenNode() {
    if (m_nodes.empty()) {
        return nullptr;
    }
    return m_nodes.front();
}
std::shared_ptr<Node> KBucket::getOldestNode() {
    if (m_nodes.empty()) {
        return nullptr;
    }
    auto oldest = m_nodes.front();
    auto oldestTime = oldest->getLastSeen();
    for (const auto& node : m_nodes) {
        if (node->getLastSeen() < oldestTime) {
            oldest = node;
            oldestTime = node->getLastSeen();
        }
    }
    return oldest;
}
// RoutingTable implementation
RoutingTable::RoutingTable(const NodeID& ownID, size_t kBucketSize)
    : m_ownID(ownID), m_kBucketSize(kBucketSize) {
    // Initialize buckets
    m_buckets.reserve(NODE_ID_BITS);
    for (size_t i = 0; i < NODE_ID_BITS; i++) {
        m_buckets.emplace_back(static_cast<int>(i), kBucketSize);
    }
}
const NodeID& RoutingTable::getOwnID() const {
    return m_ownID;
}

void RoutingTable::updateOwnID(const NodeID& ownID) {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    m_ownID = ownID;
    getLogger()->info("Updated routing table own node ID");
}
bool RoutingTable::addNode(std::shared_ptr<Node> node) {
    // Don't add ourselves
    if (node->getID() == m_ownID) {
        return false;
    }
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    return addNodeNoLock(node);
}

bool RoutingTable::addNodeNoLock(std::shared_ptr<Node> node) {
    // Don't add ourselves
    if (node->getID() == m_ownID) {
        return false;
    }
    // Get the appropriate bucket
    NodeID distance = calculateDistance(m_ownID, node->getID());
    int bucketIndex = getBucketIndex(distance);
    // If the bucket index is -1, this is our own ID
    if (bucketIndex == -1) {
        return false;
    }
    // Add the node to the bucket
    bool added = m_buckets[static_cast<size_t>(bucketIndex)].addNode(node);
    if (added) {
        getLogger()->info("Added new node to routing table: {} (bucket {})",
                     nodeIDToString(node->getID()), bucketIndex);
    }
    return added;
}
bool RoutingTable::removeNode(const NodeID& id) {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    // Get the appropriate bucket
    NodeID distance = calculateDistance(m_ownID, id);
    int bucketIndex = getBucketIndex(distance);
    // If the bucket index is -1, this is our own ID
    if (bucketIndex == -1) {
        return false;
    }
    // Find the node in the bucket
    auto node = m_buckets[static_cast<size_t>(bucketIndex)].findNode(id);
    if (!node) {
        return false;
    }
    // Remove the node from the bucket
    return m_buckets[static_cast<size_t>(bucketIndex)].removeNode(node);
}
std::shared_ptr<Node> RoutingTable::findNode(const NodeID& id) {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    return findNodeNoLock(id);
}

std::shared_ptr<Node> RoutingTable::findNodeNoLock(const NodeID& id) const {
    // Get the appropriate bucket
    NodeID distance = calculateDistance(m_ownID, id);
    int bucketIndex = getBucketIndex(distance);
    // If the bucket index is -1, this is our own ID
    if (bucketIndex == -1) {
        return nullptr;
    }
    // Find the node in the bucket
    return m_buckets[static_cast<size_t>(bucketIndex)].findNode(id);
}
KBucket& RoutingTable::getBucket(const NodeID& id) {
    NodeID distance = calculateDistance(m_ownID, id);
    int bucketIndex = getBucketIndex(distance);
    // If the bucket index is -1, this is our own ID, return the first bucket
    if (bucketIndex == -1) {
        return m_buckets[0];
    }
    return m_buckets[static_cast<size_t>(bucketIndex)];
}
KBucket& RoutingTable::getBucketByIndex(int index) {
    assert(index >= 0 && static_cast<size_t>(index) < NODE_ID_BITS);
    return m_buckets[static_cast<size_t>(index)];
}
const std::vector<KBucket>& RoutingTable::getBuckets() const {
    return m_buckets;
}
std::vector<std::shared_ptr<Node>> RoutingTable::getClosestNodes(const NodeID& id, size_t count) {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    return getClosestNodesNoLock(id, count);
}

std::vector<std::shared_ptr<Node>> RoutingTable::getClosestNodesNoLock(const NodeID& id, size_t count) const {
    // Get all nodes
    std::vector<std::shared_ptr<Node>> allNodes;
    for (const auto& bucket : m_buckets) {
        for (const auto& node : bucket.getNodes()) {
            allNodes.push_back(node);
        }
    }
    // Sort nodes by distance to the target ID
    std::sort(allNodes.begin(), allNodes.end(), [&id](const auto& a, const auto& b) {
        NodeID distanceA = calculateDistance(a->getID(), id);
        NodeID distanceB = calculateDistance(b->getID(), id);
        return std::lexicographical_compare(
            distanceA.begin(), distanceA.end(),
            distanceB.begin(), distanceB.end()
        );
    });
    // Return the closest nodes
    if (allNodes.size() > count) {
        allNodes.resize(count);
    }
    return allNodes;
}
std::vector<std::shared_ptr<Node>> RoutingTable::getAllNodes() {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    std::vector<std::shared_ptr<Node>> allNodes;
    for (const auto& bucket : m_buckets) {
        for (const auto& node : bucket.getNodes()) {
            allNodes.push_back(node);
        }
    }
    return allNodes;
}
size_t RoutingTable::size() const {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    size_t totalNodes = 0;
    for (const auto& bucket : m_buckets) {
        totalNodes += bucket.size();
    }
    return totalNodes;
}
bool RoutingTable::isEmpty() const {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    for (const auto& bucket : m_buckets) {
        if (!bucket.getNodes().empty()) {
            return false;
        }
    }
    return true;
}
void RoutingTable::clear() {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    clearNoLock();
}

void RoutingTable::clearNoLock() {
    for (auto& bucket : m_buckets) {
        bucket = KBucket(bucket.getIndex());
    }
}
} // namespace dht_hunter::dht
