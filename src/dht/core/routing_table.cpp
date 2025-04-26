#include "dht_hunter/dht/core/routing_table.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace dht_hunter::dht {

KBucket::KBucket(size_t prefix, size_t kSize)
    : m_prefix(prefix), m_kSize(kSize) {
}

KBucket::KBucket(KBucket&& other) noexcept
    : m_prefix(other.m_prefix),
      m_kSize(other.m_kSize),
      m_nodes(std::move(other.m_nodes)) {
    // Note: We don't move the mutex as that would be unsafe
    // Instead, we create a new mutex for this instance
}

KBucket& KBucket::operator=(KBucket&& other) noexcept {
    if (this != &other) {
        // Lock both mutexes to prevent data races
        std::lock_guard<std::mutex> lock_this(m_mutex);
        std::lock_guard<std::mutex> lock_other(other.m_mutex);

        m_prefix = other.m_prefix;
        m_kSize = other.m_kSize;
        m_nodes = std::move(other.m_nodes);
        // Note: We don't move the mutex as that would be unsafe
    }
    return *this;
}

bool KBucket::addNode(std::shared_ptr<Node> node) {
    if (!node) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if the node is already in the bucket
    auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
        [&node](const std::shared_ptr<Node>& existingNode) {
            return existingNode->getID() == node->getID();
        });

    if (it != m_nodes.end()) {
        // Node already exists, move it to the end (most recently seen)
        (*it)->updateLastSeen();
        std::rotate(it, it + 1, m_nodes.end());
        return true;
    }

    // If the bucket is not full, add the node
    if (m_nodes.size() < m_kSize) {
        m_nodes.push_back(node);
        return true;
    }

    // Bucket is full, can't add the node
    return false;
}

bool KBucket::removeNode(const NodeID& nodeID) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
        [&nodeID](const std::shared_ptr<Node>& node) {
            return node->getID() == nodeID;
        });

    if (it != m_nodes.end()) {
        m_nodes.erase(it);
        return true;
    }

    return false;
}

std::shared_ptr<Node> KBucket::getNode(const NodeID& nodeID) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
        [&nodeID](const std::shared_ptr<Node>& node) {
            return node->getID() == nodeID;
        });

    if (it != m_nodes.end()) {
        return *it;
    }

    return nullptr;
}

std::vector<std::shared_ptr<Node>> KBucket::getNodes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_nodes;
}

size_t KBucket::getNodeCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_nodes.size();
}

bool KBucket::containsNodeID(const NodeID& nodeID, const NodeID& ownID) const {
    // Calculate the XOR distance between the node ID and the own ID
    NodeID distance = nodeID.distanceTo(ownID);

    // Check if the first m_prefix bits of the distance are 0
    for (size_t i = 0; i < m_prefix / 8; ++i) {
        if (distance[i] != 0) {
            return false;
        }
    }

    // Check the remaining bits in the last byte
    if (m_prefix % 8 != 0) {
        uint8_t mask = static_cast<uint8_t>(0xFF << (8 - static_cast<int>(m_prefix % 8)));
        if ((distance[m_prefix / 8] & mask) != 0) {
            return false;
        }
    }

    return true;
}

size_t KBucket::getPrefix() const {
    return m_prefix;
}

// Initialize static members
std::shared_ptr<RoutingTable> RoutingTable::s_instance = nullptr;
std::mutex RoutingTable::s_instanceMutex;

std::shared_ptr<RoutingTable> RoutingTable::getInstance(const NodeID& ownID, size_t kBucketSize) {
    std::lock_guard<std::mutex> lock(s_instanceMutex);

    if (!s_instance) {
        s_instance = std::shared_ptr<RoutingTable>(new RoutingTable(ownID, kBucketSize));
    }

    return s_instance;
}

RoutingTable::RoutingTable(const NodeID& ownID, size_t kBucketSize)
    : m_ownID(ownID), m_kBucketSize(kBucketSize), m_logger(event::Logger::forComponent("DHT.RoutingTable")) {
    // Start with a single bucket with prefix 0
    m_buckets.emplace_back(0, kBucketSize);
    m_logger.info("Routing table created with node ID: {}", nodeIDToString(ownID));
}

RoutingTable::~RoutingTable() {
    // Clear the singleton instance
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (s_instance.get() == this) {
        s_instance.reset();
    }
}

bool RoutingTable::addNode(const std::shared_ptr<Node> &node) {
    if (!node || !node->getID().isValid() || node->getID() == m_ownID) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Find the appropriate bucket
    KBucket& bucket = getBucketForNodeID(node->getID());

    // Try to add the node to the bucket
    if (bucket.addNode(node)) {
        return true;
    }

    // If the bucket is full, check if it needs to be split
    if (bucket.containsNodeID(m_ownID, m_ownID)) {
        // This bucket contains our own node ID, split it
        size_t bucketIndex = static_cast<size_t>(&bucket - &m_buckets[0]);
        splitBucket(bucketIndex);

        // Try to add the node again
        return addNode(node);
    }

    // Bucket is full and doesn't contain our own node ID, can't add the node
    m_logger.debug("Failed to add node {} to routing table (bucket full)", nodeIDToString(node->getID()));
    return false;
}

bool RoutingTable::removeNode(const NodeID& nodeID) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find the appropriate bucket
    KBucket& bucket = const_cast<KBucket&>(getBucketForNodeID(nodeID));

    // Try to remove the node from the bucket
    if (bucket.removeNode(nodeID)) {
        m_logger.debug("Removed node {} from routing table", nodeIDToString(nodeID));
        return true;
    }

    return false;
}

std::shared_ptr<Node> RoutingTable::getNode(const NodeID& nodeID) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find the appropriate bucket
    const KBucket& bucket = getBucketForNodeID(nodeID);

    // Try to get the node from the bucket
    return bucket.getNode(nodeID);
}

std::vector<std::shared_ptr<Node>> RoutingTable::getClosestNodes(const NodeID& targetID, size_t k) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Get all nodes
    std::vector<std::shared_ptr<Node>> allNodes;
    for (const auto& bucket : m_buckets) {
        auto bucketNodes = bucket.getNodes();
        allNodes.insert(allNodes.end(), bucketNodes.begin(), bucketNodes.end());
    }

    // Sort nodes by distance to target ID
    std::sort(allNodes.begin(), allNodes.end(),
        [&targetID](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
            NodeID distanceA = a->getID().distanceTo(targetID);
            NodeID distanceB = b->getID().distanceTo(targetID);
            return distanceA < distanceB;
        });

    // Return the k closest nodes
    if (allNodes.size() > k) {
        allNodes.resize(k);
    }

    return allNodes;
}

std::vector<std::shared_ptr<Node>> RoutingTable::getAllNodes() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::shared_ptr<Node>> allNodes;
    for (const auto& bucket : m_buckets) {
        auto bucketNodes = bucket.getNodes();
        allNodes.insert(allNodes.end(), bucketNodes.begin(), bucketNodes.end());
    }

    return allNodes;
}

size_t RoutingTable::getNodeCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t count = 0;
    for (const auto& bucket : m_buckets) {
        count += bucket.getNodeCount();
    }

    return count;
}

bool RoutingTable::saveToFile(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        m_logger.error("Failed to open file for writing: {}", filePath);
        return false;
    }

    // Write the number of nodes
    size_t nodeCount = getNodeCount();
    file.write(reinterpret_cast<const char*>(&nodeCount), sizeof(nodeCount));

    // Write each node
    for (const auto& bucket : m_buckets) {
        for (const auto& node : bucket.getNodes()) {
            // Write the node ID
            file.write(reinterpret_cast<const char*>(node->getID().data()), static_cast<std::streamsize>(node->getID().size()));

            // Write the endpoint
            const auto& endpoint = node->getEndpoint();
            std::string address = endpoint.getAddress().toString();
            uint16_t port = endpoint.getPort();

            // Write the address length and address
            uint16_t addressLength = static_cast<uint16_t>(address.length());
            file.write(reinterpret_cast<const char*>(&addressLength), sizeof(addressLength));
            file.write(address.c_str(), addressLength);

            // Write the port
            file.write(reinterpret_cast<const char*>(&port), sizeof(port));
        }
    }

    m_logger.info("Saved routing table to file: {}", filePath);
    return true;
}

bool RoutingTable::loadFromFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        m_logger.error("Failed to open file for reading: {}", filePath);
        return false;
    }

    // Clear the routing table
    m_buckets.clear();
    m_buckets.emplace_back(0, m_kBucketSize);

    // Read the number of nodes
    size_t nodeCount;
    file.read(reinterpret_cast<char*>(&nodeCount), sizeof(nodeCount));

    // Read each node
    for (size_t i = 0; i < nodeCount; ++i) {
        // Read the node ID
        NodeID nodeID;
        file.read(reinterpret_cast<char*>(nodeID.data()), static_cast<std::streamsize>(nodeID.size()));

        // Read the address length and address
        uint16_t addressLength;
        file.read(reinterpret_cast<char*>(&addressLength), sizeof(addressLength));

        std::string address(addressLength, '\0');
        file.read(&address[0], addressLength);

        // Read the port
        uint16_t port;
        file.read(reinterpret_cast<char*>(&port), sizeof(port));

        // Create the node
        network::NetworkAddress networkAddress(address);
        network::EndPoint endpoint(networkAddress, port);
        auto node = std::make_shared<Node>(nodeID, endpoint);

        // Add the node to the routing table
        addNode(node);
    }

    m_logger.info("Loaded routing table from file: {}", filePath);
    return true;
}

KBucket& RoutingTable::getBucketForNodeID(const NodeID& nodeID) {
    // Find the appropriate bucket
    for (size_t i = 0; i < m_buckets.size(); ++i) {
        if (m_buckets[i].containsNodeID(nodeID, m_ownID)) {
            return m_buckets[i];
        }
    }

    // If no bucket is found, return the last bucket
    return m_buckets.back();
}

const KBucket& RoutingTable::getBucketForNodeID(const NodeID& nodeID) const {
    // Find the appropriate bucket
    for (size_t i = 0; i < m_buckets.size(); ++i) {
        if (m_buckets[i].containsNodeID(nodeID, m_ownID)) {
            return m_buckets[i];
        }
    }

    // If no bucket is found, return the last bucket
    return m_buckets.back();
}

void RoutingTable::splitBucket(size_t bucketIndex) {
    if (bucketIndex >= m_buckets.size()) {
        return;
    }

    // Get the bucket to split
    KBucket& bucket = m_buckets[bucketIndex];

    // Create two new buckets
    size_t newPrefix = bucket.getPrefix() + 1;
    KBucket newBucket1(newPrefix, m_kBucketSize);
    KBucket newBucket2(newPrefix, m_kBucketSize);

    // Redistribute nodes
    for (const auto& node : bucket.getNodes()) {
        if (newBucket1.containsNodeID(node->getID(), m_ownID)) {
            newBucket1.addNode(node);
        } else {
            newBucket2.addNode(node);
        }
    }

    // Replace the old bucket with the new buckets
    m_buckets.erase(m_buckets.begin() + static_cast<std::ptrdiff_t>(bucketIndex));
    m_buckets.insert(m_buckets.begin() + static_cast<std::ptrdiff_t>(bucketIndex), std::move(newBucket1));
    m_buckets.insert(m_buckets.begin() + static_cast<std::ptrdiff_t>(bucketIndex) + 1, std::move(newBucket2));

    m_logger.debug("Split bucket at index {} into two buckets with prefix {}", bucketIndex, newPrefix);
}

} // namespace dht_hunter::dht
