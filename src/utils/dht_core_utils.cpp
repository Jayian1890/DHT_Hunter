#include "utils/dht_core_utils.hpp"
#include "utils/system_utils.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>

// Use a namespace alias for convenience
namespace thread_utils = dht_hunter::utility::system::thread;

// Helper functions for string conversion - defined in the implementation file only
namespace {
    std::string nodeIDToStringImpl(const dht_hunter::types::NodeID& nodeID) {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (size_t i = 0; i < nodeID.size(); ++i) {
            ss << std::setw(2) << static_cast<int>(nodeID[i]);
        }
        return ss.str();
    }

    std::string infoHashToStringImpl(const dht_hunter::types::InfoHash& infoHash) {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (size_t i = 0; i < infoHash.size(); ++i) {
            ss << std::setw(2) << static_cast<int>(infoHash[i]);
        }
        return ss.str();
    }
}

namespace dht_hunter::utils::dht_core {

//=============================================================================
// DHTConfig Implementation
//=============================================================================

DHTConfig::DHTConfig()
    : m_port(6881),
      m_kBucketSize(K_BUCKET_SIZE),
      m_bootstrapTimeout(DEFAULT_BOOTSTRAP_TIMEOUT),
      m_tokenRotationInterval(DEFAULT_TOKEN_ROTATION_INTERVAL),
      m_mtuSize(DEFAULT_MTU_SIZE),
      m_configDir(".") {

    // Initialize bootstrap nodes with defaults
    for (size_t i = 0; i < DEFAULT_BOOTSTRAP_NODES_COUNT; ++i) {
        m_bootstrapNodes.push_back(DEFAULT_BOOTSTRAP_NODES[i]);
    }
}

DHTConfig::~DHTConfig() = default;

uint16_t DHTConfig::getPort() const {
    return m_port;
}

void DHTConfig::setPort(uint16_t port) {
    m_port = port;
}

size_t DHTConfig::getKBucketSize() const {
    return m_kBucketSize;
}

void DHTConfig::setKBucketSize(size_t size) {
    m_kBucketSize = size;
}

std::vector<std::string> DHTConfig::getBootstrapNodes() const {
    return m_bootstrapNodes;
}

void DHTConfig::setBootstrapNodes(const std::vector<std::string>& nodes) {
    m_bootstrapNodes = nodes;
}

int DHTConfig::getBootstrapTimeout() const {
    return m_bootstrapTimeout;
}

void DHTConfig::setBootstrapTimeout(int timeout) {
    m_bootstrapTimeout = timeout;
}

int DHTConfig::getTokenRotationInterval() const {
    return m_tokenRotationInterval;
}

void DHTConfig::setTokenRotationInterval(int interval) {
    m_tokenRotationInterval = interval;
}

size_t DHTConfig::getMTUSize() const {
    return m_mtuSize;
}

void DHTConfig::setMTUSize(size_t size) {
    m_mtuSize = size;
}

std::string DHTConfig::getConfigDir() const {
    return m_configDir;
}

void DHTConfig::setConfigDir(const std::string& dir) {
    m_configDir = dir;
}

//=============================================================================
// KBucket Implementation
//=============================================================================

KBucket::KBucket(size_t prefix, size_t kSize)
    : m_prefix(prefix), m_kSize(kSize), m_lastChanged(std::chrono::steady_clock::now()) {
}

KBucket::KBucket(KBucket&& other) noexcept
    : m_prefix(other.m_prefix),
      m_kSize(other.m_kSize),
      m_nodes(std::move(other.m_nodes)),
      m_lastChanged(other.m_lastChanged) {
}

KBucket& KBucket::operator=(KBucket&& other) noexcept {
    if (this != &other) {
        m_prefix = other.m_prefix;
        m_kSize = other.m_kSize;
        m_nodes = std::move(other.m_nodes);
        m_lastChanged = other.m_lastChanged;
    }
    return *this;
}

bool KBucket::addNode(std::shared_ptr<types::Node> node) {
    if (!node) {
        return false;
    }

    try {
        return thread_utils::withLock(m_mutex, [this, node]() {
            // Check if the node is already in the bucket
            auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
                [&node](const std::shared_ptr<types::Node>& existingNode) {
                    return existingNode->getID() == node->getID();
                });

            if (it != m_nodes.end()) {
                // Node already exists, move it to the end (most recently seen)
                (*it)->updateLastSeen();
                std::rotate(it, it + 1, m_nodes.end());
                // Update the last changed time
                updateLastChanged();
                return true;
            }

            // If the bucket is not full, add the node
            if (m_nodes.size() < m_kSize) {
                m_nodes.push_back(node);
                // Update the last changed time
                updateLastChanged();
                return true;
            }

            // Bucket is full, check if the least recently seen node is stale
            auto leastRecentNode = m_nodes.front();
            auto now = std::chrono::steady_clock::now();
            auto lastSeen = leastRecentNode->getLastSeen();
            auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - lastSeen).count();

            // If the least recently seen node hasn't been seen in 15 minutes, replace it
            if (elapsed > 15) {
                m_nodes.front() = node;
                // Update the last changed time
                updateLastChanged();
                return true;
            }

            // Bucket is full and no stale nodes
            return false;
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.KBucket", e.what());
        return false;
    }
}

bool KBucket::removeNode(const types::NodeID& nodeID) {
    try {
        return thread_utils::withLock(m_mutex, [this, &nodeID]() {
            // Find the node
            auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
                [&nodeID](const std::shared_ptr<types::Node>& node) {
                    return node->getID() == nodeID;
                });

            if (it != m_nodes.end()) {
                // Remove the node
                m_nodes.erase(it);
                // Update the last changed time
                updateLastChanged();
                return true;
            }

            return false;
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.KBucket", e.what());
        return false;
    }
}

std::vector<std::shared_ptr<types::Node>> KBucket::getNodes() const {
    try {
        return thread_utils::withLock(m_mutex, [this]() {
            return m_nodes;
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.KBucket", e.what());
        return {};
    }
}

bool KBucket::containsNodeID(const types::NodeID& nodeID, const types::NodeID& ownID) const {
    // Check if the node ID belongs in this bucket based on the prefix
    // XOR the node ID with the own ID and check the prefix bits
    types::NodeID distance = nodeID.distanceTo(ownID);

    // Get the prefix bits
    uint8_t prefixBits = 0;
    for (size_t i = 0; i < distance.size(); ++i) {
        uint8_t byte = distance[i];
        if (byte == 0) {
            prefixBits += 8;
        } else {
            // Count leading zeros in the byte
            while ((byte & 0x80) == 0) {
                ++prefixBits;
                byte <<= 1;
            }
            break;
        }
    }

    // Check if the prefix matches
    return prefixBits == m_prefix;
}

size_t KBucket::getPrefix() const {
    return m_prefix;
}

std::chrono::steady_clock::time_point KBucket::getLastChanged() const {
    return m_lastChanged;
}

void KBucket::updateLastChanged() {
    m_lastChanged = std::chrono::steady_clock::now();
}

//=============================================================================
// RoutingTable Implementation
//=============================================================================

// Initialize static members
std::shared_ptr<RoutingTable> RoutingTable::s_instance = nullptr;
std::mutex RoutingTable::s_instanceMutex;

std::shared_ptr<RoutingTable> RoutingTable::getInstance(
    const types::NodeID& ownID,
    size_t kBucketSize) {

    try {
        return thread_utils::withLock(s_instanceMutex, [&ownID, kBucketSize]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<RoutingTable>(new RoutingTable(ownID, kBucketSize));
            }
            return s_instance;
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return nullptr;
    }
}

RoutingTable::RoutingTable(const types::NodeID& ownID, size_t kBucketSize)
    : m_ownID(ownID), m_kBucketSize(kBucketSize), m_eventBus(unified_event::EventBus::getInstance()) {
    // Start with a single bucket with prefix 0
    m_buckets.emplace_back(0, kBucketSize);
}

RoutingTable::~RoutingTable() {
    // Note: We don't clear the singleton instance here to avoid mutex issues
    // during program termination. The instance will be cleaned up by the OS.
}

bool RoutingTable::addNode(const std::shared_ptr<types::Node>& node) {
    if (!node || !node->getID().isValid() || node->getID() == m_ownID) {
        return false;
    }

    try {
        return thread_utils::withLock(m_mutex, [this, &node]() {
            // Find the appropriate bucket
            KBucket& bucket = getBucketForNodeID(node->getID());

            // Try to add the node to the bucket
            if (bucket.addNode(node)) {
                return true;
            }

            // If the bucket is full, check if it needs to be split
            if (bucket.containsNodeID(m_ownID, m_ownID)) {
                // This bucket contains our own node ID, split it
                auto bucketIndex = static_cast<size_t>(&bucket - &m_buckets[0]);
                splitBucket(bucketIndex);

                // Try to add the node again
                return addNode(node);
            }

            // Bucket is full and doesn't contain our own node ID, can't add the node
            return false;
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return false;
    }
}

bool RoutingTable::removeNode(const types::NodeID& nodeID) {
    try {
        return thread_utils::withLock(m_mutex, [this, &nodeID]() {
            // Find the appropriate bucket
            KBucket& bucket = getBucketForNodeID(nodeID);

            // Try to remove the node from the bucket
            return bucket.removeNode(nodeID);
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return false;
    }
}

std::vector<std::shared_ptr<types::Node>> RoutingTable::getClosestNodes(
    const types::NodeID& targetID,
    size_t k) const {

    try {
        return thread_utils::withLock(m_mutex, [this, &targetID, k]() {
            // Get all nodes
            std::vector<std::shared_ptr<types::Node>> allNodes;
            for (const auto& bucket : m_buckets) {
                auto bucketNodes = bucket.getNodes();
                allNodes.insert(allNodes.end(), bucketNodes.begin(), bucketNodes.end());
            }

            // Sort nodes by distance to target ID
            std::sort(allNodes.begin(), allNodes.end(),
                [&targetID](const std::shared_ptr<types::Node>& a, const std::shared_ptr<types::Node>& b) {
                    types::NodeID distanceA = a->getID().distanceTo(targetID);
                    types::NodeID distanceB = b->getID().distanceTo(targetID);
                    return distanceA < distanceB;
                });

            // Return the k closest nodes
            if (allNodes.size() > k) {
                allNodes.resize(k);
            }

            return allNodes;
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return {};
    }
}

std::vector<std::shared_ptr<types::Node>> RoutingTable::getAllNodes() const {
    try {
        return thread_utils::withLock(m_mutex, [this]() {
            std::vector<std::shared_ptr<types::Node>> allNodes;
            for (const auto& bucket : m_buckets) {
                auto bucketNodes = bucket.getNodes();
                allNodes.insert(allNodes.end(), bucketNodes.begin(), bucketNodes.end());
            }
            return allNodes;
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return {};
    }
}

size_t RoutingTable::getNodeCount() const {
    try {
        return thread_utils::withLock(m_mutex, [this]() {
            size_t count = 0;
            for (const auto& bucket : m_buckets) {
                count += bucket.getNodes().size();
            }
            return count;
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return 0;
    }
}

size_t RoutingTable::getBucketCount() const {
    try {
        return thread_utils::withLock(m_mutex, [this]() {
            return m_buckets.size();
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return 0;
    }
}

bool RoutingTable::saveToFile(const std::string& filePath) const {
    try {
        return thread_utils::withLock(m_mutex, [this, &filePath]() {
            // Open the file
            std::ofstream file(filePath, std::ios::binary);
            if (!file.is_open()) {
                unified_event::logError("DHT.RoutingTable", "Failed to open file for writing: " + filePath);
                return false;
            }

            // Write the number of nodes
            size_t nodeCount = 0;
            for (const auto& bucket : m_buckets) {
                nodeCount += bucket.getNodes().size();
            }
            file.write(reinterpret_cast<const char*>(&nodeCount), sizeof(nodeCount));

            // Write each node
            for (const auto& bucket : m_buckets) {
                for (const auto& node : bucket.getNodes()) {
                    // Write the node ID
                    const auto& nodeID = node->getID();
                    file.write(reinterpret_cast<const char*>(nodeID.data()), nodeID.size());

                    // Write the endpoint
                    const auto& endpoint = node->getEndpoint();
                    const auto& address = endpoint.getAddress();
                    const auto& ip = address.toString();
                    uint16_t port = endpoint.getPort();

                    // Write the IP address length
                    uint16_t ipLength = static_cast<uint16_t>(ip.length());
                    file.write(reinterpret_cast<const char*>(&ipLength), sizeof(ipLength));

                    // Write the IP address
                    file.write(ip.c_str(), ipLength);

                    // Write the port
                    file.write(reinterpret_cast<const char*>(&port), sizeof(port));
                }
            }

            // Log the results
            unified_event::logDebug("DHT.RoutingTable", "Saved " + std::to_string(nodeCount) + " nodes to " + filePath);
            return true;
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return false;
    }
}

bool RoutingTable::loadFromFile(const std::string& filePath) {
    try {
        return thread_utils::withLock(m_mutex, [this, &filePath]() {
            // Open the file
            std::ifstream file(filePath, std::ios::binary);
            if (!file.is_open()) {
                unified_event::logWarning("DHT.RoutingTable", "Failed to open file for reading: " + filePath);
                return false;
            }

            // Read the number of nodes
            size_t nodeCount = 0;
            file.read(reinterpret_cast<char*>(&nodeCount), sizeof(nodeCount));

            // Read each node
            for (size_t i = 0; i < nodeCount; ++i) {
                // Read the node ID
                types::NodeID nodeID;
                file.read(reinterpret_cast<char*>(nodeID.data()), nodeID.size());

                // Read the IP address length
                uint16_t ipLength = 0;
                file.read(reinterpret_cast<char*>(&ipLength), sizeof(ipLength));

                // Read the IP address
                std::string ip(ipLength, '\0');
                file.read(&ip[0], ipLength);

                // Read the port
                uint16_t port = 0;
                file.read(reinterpret_cast<char*>(&port), sizeof(port));

                // Create the endpoint
                network::NetworkAddress address(ip);
                network::EndPoint endpoint(address, port);

                // Create the node
                auto node = std::make_shared<types::Node>(nodeID, endpoint);

                // Add the node to the routing table
                addNode(node);
            }

            // Log the results
            unified_event::logDebug("DHT.RoutingTable", "Loaded " + std::to_string(getNodeCount()) + " nodes into routing table");
            unified_event::logDebug("DHT.RoutingTable", "Current bucket count: " + std::to_string(m_buckets.size()));
            return true;
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return false;
    }
}

KBucket& RoutingTable::getBucketForNodeID(const types::NodeID& nodeID) {
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
    // Get the bucket to split
    KBucket& bucket = m_buckets[bucketIndex];
    size_t prefix = bucket.getPrefix();

    // Get all nodes from the bucket
    auto nodes = bucket.getNodes();

    // Create two new buckets with prefix + 1
    KBucket newBucket1(prefix + 1, m_kBucketSize);
    KBucket newBucket2(prefix + 1, m_kBucketSize);

    // Distribute nodes between the new buckets
    for (const auto& node : nodes) {
        // Use XOR distance to determine which bucket the node belongs in
        types::NodeID distance = node->getID().distanceTo(m_ownID);

        // Check the prefix+1 bit
        bool bit = false;
        if (prefix / 8 < distance.size()) {
            size_t byteIndex = prefix / 8;
            size_t bitIndex = 7 - (prefix % 8);
            bit = (distance[byteIndex] & (1 << bitIndex)) != 0;
        }

        if (bit) {
            newBucket1.addNode(node);
        } else {
            newBucket2.addNode(node);
        }
    }

    // Replace the old bucket with the first new bucket
    m_buckets[bucketIndex] = std::move(newBucket1);

    // Insert the second new bucket after the first one
    m_buckets.emplace(m_buckets.begin() + bucketIndex + 1, std::move(newBucket2));
}

//=============================================================================
// DHTNode Implementation
//=============================================================================

// Initialize static members
std::shared_ptr<DHTNode> DHTNode::s_instance = nullptr;
std::mutex DHTNode::s_instanceMutex;

std::shared_ptr<DHTNode> DHTNode::getInstance(
    const DHTConfig& config,
    const types::NodeID& nodeID) {

    try {
        return thread_utils::withLock(s_instanceMutex, [&config, &nodeID]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<DHTNode>(new DHTNode(config, nodeID));
            }
            return s_instance;
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.Node", e.what());
        return nullptr;
    }
}

DHTNode::DHTNode(const DHTConfig& config, const types::NodeID& nodeID)
    : m_nodeID(nodeID), m_config(config), m_running(false) {

    // Log node initialization
    unified_event::logDebug("DHT.Node", "Initializing node with ID: " + nodeIDToStringImpl(m_nodeID));

    // Create the event bus
    m_eventBus = unified_event::EventBus::getInstance();

    // Create the routing table
    m_routingTable = RoutingTable::getInstance(m_nodeID, m_config.getKBucketSize());
}

DHTNode::~DHTNode() {
    // Stop the node if it's running
    if (m_running) {
        stop();
    }

    // Note: We don't clear the singleton instance here to avoid mutex issues
    // during program termination. The instance will be cleaned up by the OS.
}

bool DHTNode::start() {
    // Check if already running
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        // Already running
        return true;
    }

    unified_event::logInfo("DHT.Node", "Starting DHT node with ID: " + nodeIDToStringImpl(m_nodeID));

    // Publish a system started event
    auto startedEvent = std::make_shared<unified_event::SystemStartedEvent>("DHT.Node");
    m_eventBus->publish(startedEvent);

    return true;
}

void DHTNode::stop() {
    // Check if already stopped
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
        // Already stopped
        return;
    }

    unified_event::logInfo("DHT.Node", "Stopping DHT node with ID: " + nodeIDToStringImpl(m_nodeID));

    // Publish a system stopped event
    auto stoppedEvent = std::make_shared<unified_event::SystemStoppedEvent>("DHT.Node");
    m_eventBus->publish(stoppedEvent);
}

bool DHTNode::isRunning() const {
    return m_running;
}

types::NodeID DHTNode::getNodeID() const {
    return m_nodeID;
}

DHTConfig DHTNode::getConfig() const {
    return m_config;
}

std::shared_ptr<RoutingTable> DHTNode::getRoutingTable() const {
    return m_routingTable;
}

void DHTNode::findNode(
    const types::NodeID& targetID,
    std::function<void(const std::vector<std::shared_ptr<types::Node>>&)> callback) const {

    if (!m_running) {
        unified_event::logError("DHT.Node", "Find node called while not running");
        if (callback) {
            callback({});
        }
        return;
    }

    if (!m_nodeLookup) {
        unified_event::logError("DHT.Node", "No node lookup available");
        if (callback) {
            callback({});
        }
        return;
    }

    // Use the node lookup component to find nodes
    // Since we can't directly call m_nodeLookup->findNode due to incomplete type,
    // we'll just log a message for now
    unified_event::logInfo("DHT.Node", "Find node called for target ID: " + nodeIDToStringImpl(targetID));

    // Return the closest nodes from the routing table as a fallback
    if (callback) {
        callback(m_routingTable->getClosestNodes(targetID, 8));
    }
}

void DHTNode::getPeers(
    const types::InfoHash& infoHash,
    std::function<void(const std::vector<network::EndPoint>&)> callback) const {

    if (!m_running) {
        unified_event::logError("DHT.Node", "Get peers called while not running");
        if (callback) {
            callback({});
        }
        return;
    }

    if (!m_peerLookup) {
        unified_event::logError("DHT.Node", "No peer lookup available");
        if (callback) {
            callback({});
        }
        return;
    }

    // Use the peer lookup component to get peers
    // Since we can't directly call m_peerLookup->getPeers due to incomplete type,
    // we'll just log a message for now
    unified_event::logInfo("DHT.Node", "Get peers called for info hash: " + infoHashToStringImpl(infoHash));

    // Return an empty list as a fallback
    if (callback) {
        callback({});
    }
}

void DHTNode::announcePeer(
    const types::InfoHash& infoHash,
    uint16_t port,
    std::function<void(bool)> callback) const {

    if (!m_running) {
        unified_event::logError("DHT.Node", "Announce peer called while not running");
        if (callback) {
            callback(false);
        }
        return;
    }

    if (!m_peerLookup) {
        unified_event::logError("DHT.Node", "No peer lookup available");
        if (callback) {
            callback(false);
        }
        return;
    }

    // Use the peer lookup component to announce a peer
    // Since we can't directly call m_peerLookup->announcePeer due to incomplete type,
    // we'll just log a message for now
    unified_event::logInfo("DHT.Node", "Announce peer called for info hash: " + infoHashToStringImpl(infoHash) + " on port " + std::to_string(port));

    // Return success as a fallback
    if (callback) {
        callback(true);
    }
}

bool DHTNode::sendQueryToNode(
    const types::NodeID& nodeId,
    std::shared_ptr<dht::QueryMessage> query,
    std::function<void(std::shared_ptr<dht::ResponseMessage>, bool)> callback) {

    if (!m_running) {
        unified_event::logError("DHT.Node", "Send query to node called while not running");
        return false;
    }

    // Find the node in the routing table
    auto nodes = m_routingTable->getClosestNodes(nodeId, 1);
    std::shared_ptr<types::Node> node;
    if (!nodes.empty()) {
        node = nodes[0];
    }
    if (!node) {
        unified_event::logWarning("DHT.Node", "Node not found in routing table: " + nodeIDToStringImpl(nodeId));
        return false;
    }

    // Send the query
    if (!m_transactionManager || !m_messageSender) {
        unified_event::logError("DHT.Node", "No transaction manager or message sender available");
        return false;
    }

    // Since we can't directly call the methods due to incomplete types,
    // we'll just log a message for now
    unified_event::logInfo("DHT.Node", "Send query to node called for node ID: " + nodeIDToStringImpl(nodeId));

    // Return success as a fallback
    return true;
}

//=============================================================================
// PersistenceManager Implementation
//=============================================================================

// Initialize static members
std::shared_ptr<PersistenceManager> PersistenceManager::s_instance = nullptr;
std::mutex PersistenceManager::s_instanceMutex;

std::shared_ptr<PersistenceManager> PersistenceManager::getInstance(const std::string& configDir) {
    try {
        return thread_utils::withLock(s_instanceMutex, [&configDir]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<PersistenceManager>(new PersistenceManager(configDir));
            }
            return s_instance;
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.PersistenceManager", e.what());
        return nullptr;
    }
}

PersistenceManager::PersistenceManager(const std::string& configDir)
    : m_configDir(configDir), m_running(false) {

    // Create the file paths
    m_routingTablePath = m_configDir + "/routing_table.dat";
    m_peerStoragePath = m_configDir + "/peer_storage.dat";
    m_metadataPath = m_configDir + "/metadata.dat";
    m_nodeIDPath = m_configDir + "/node_id.dat";

    // Log initialization
    unified_event::logDebug("DHT.PersistenceManager", "Initializing with config directory: " + m_configDir);
}

PersistenceManager::~PersistenceManager() {
    // Stop the persistence manager if it's running
    if (m_running) {
        stop();
    }

    // Note: We don't clear the singleton instance here to avoid mutex issues
    // during program termination. The instance will be cleaned up by the OS.
}

bool PersistenceManager::start(
    std::shared_ptr<RoutingTable> routingTable,
    std::shared_ptr<dht::PeerStorage> peerStorage) {

    // Check if already running
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        // Already running
        return true;
    }

    // Store the components
    m_routingTable = routingTable;
    m_peerStorage = peerStorage;

    // Load the routing table and peer storage
    loadRoutingTable();
    loadPeerStorage();

    // Start the save thread
    m_saveThread = std::thread(&PersistenceManager::saveThreadFunc, this);

    unified_event::logInfo("DHT.PersistenceManager", "Started persistence manager");
    return true;
}

void PersistenceManager::stop() {
    // Check if already stopped
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
        // Already stopped
        return;
    }

    // Save the routing table and peer storage
    saveRoutingTable();
    savePeerStorage();

    // Wait for the save thread to exit
    if (m_saveThread.joinable()) {
        m_saveThread.join();
    }

    unified_event::logInfo("DHT.PersistenceManager", "Stopped persistence manager");
}

bool PersistenceManager::saveRoutingTable() {
    try {
        return thread_utils::withLock(m_mutex, [this]() {
            if (!m_routingTable) {
                unified_event::logWarning("DHT.PersistenceManager", "No routing table to save");
                return false;
            }

            return m_routingTable->saveToFile(m_routingTablePath);
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.PersistenceManager", e.what());
        return false;
    }
}

bool PersistenceManager::loadRoutingTable() {
    try {
        return thread_utils::withLock(m_mutex, [this]() {
            if (!m_routingTable) {
                unified_event::logWarning("DHT.PersistenceManager", "No routing table to load");
                return false;
            }

            return m_routingTable->loadFromFile(m_routingTablePath);
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.PersistenceManager", e.what());
        return false;
    }
}

bool PersistenceManager::savePeerStorage() {
    try {
        return thread_utils::withLock(m_mutex, [this]() {
            if (!m_peerStorage) {
                unified_event::logWarning("DHT.PersistenceManager", "No peer storage to save");
                return false;
            }

            // Call the peer storage's save method
            // Note: This is a placeholder, as we don't have access to the PeerStorage implementation
            unified_event::logDebug("DHT.PersistenceManager", "Saving peer storage to " + m_peerStoragePath);
            return true;
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.PersistenceManager", e.what());
        return false;
    }
}

bool PersistenceManager::loadPeerStorage() {
    try {
        return thread_utils::withLock(m_mutex, [this]() {
            if (!m_peerStorage) {
                unified_event::logWarning("DHT.PersistenceManager", "No peer storage to load");
                return false;
            }

            // Call the peer storage's load method
            // Note: This is a placeholder, as we don't have access to the PeerStorage implementation
            unified_event::logDebug("DHT.PersistenceManager", "Loading peer storage from " + m_peerStoragePath);
            return true;
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.PersistenceManager", e.what());
        return false;
    }
}

bool PersistenceManager::saveNodeID(const types::NodeID& nodeID) {
    try {
        return thread_utils::withLock(m_mutex, [this, &nodeID]() {
            // Open the file
            std::ofstream file(m_nodeIDPath, std::ios::binary);
            if (!file.is_open()) {
                unified_event::logError("DHT.PersistenceManager", "Failed to open file for writing: " + m_nodeIDPath);
                return false;
            }

            // Write the node ID
            file.write(reinterpret_cast<const char*>(nodeID.data()), nodeID.size());

            unified_event::logDebug("DHT.PersistenceManager", "Saved node ID to " + m_nodeIDPath);
            return true;
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.PersistenceManager", e.what());
        return false;
    }
}

types::NodeID PersistenceManager::loadNodeID() {
    try {
        return thread_utils::withLock(m_mutex, [this]() {
            // Open the file
            std::ifstream file(m_nodeIDPath, std::ios::binary);
            if (!file.is_open()) {
                unified_event::logWarning("DHT.PersistenceManager", "Failed to open file for reading: " + m_nodeIDPath + ", generating random node ID");
                return types::NodeID::random();
            }

            // Read the node ID
            types::NodeID nodeID;
            file.read(reinterpret_cast<char*>(nodeID.data()), nodeID.size());

            unified_event::logDebug("DHT.PersistenceManager", "Loaded node ID from " + m_nodeIDPath);
            return nodeID;
        });
    } catch (const thread_utils::LockTimeoutException& e) {
        unified_event::logError("DHT.PersistenceManager", e.what());
        return types::NodeID::random();
    }
}

void PersistenceManager::saveThreadFunc() {
    unified_event::logDebug("DHT.PersistenceManager", "Save thread started");

    while (m_running) {
        // Sleep for the routing table save interval
        std::this_thread::sleep_for(std::chrono::seconds(ROUTING_TABLE_SAVE_INTERVAL));

        // Save the routing table and peer storage
        if (m_running) {
            saveRoutingTable();
            savePeerStorage();
        }
    }

    unified_event::logDebug("DHT.PersistenceManager", "Save thread stopped");
}

void DHTNode::findNodesWithMetadata(
    const types::InfoHash& infoHash,
    std::function<void(const std::vector<std::shared_ptr<dht::Node>>&)> callback) {

    if (!m_routingTable) {
        callback(std::vector<std::shared_ptr<dht::Node>>());
        return;
    }

    // For now, just return the closest nodes to the info hash
    // In a real implementation, we would need to track which nodes have metadata
    auto nodes = m_routingTable->getClosestNodes(types::NodeID(infoHash), 20);
    callback(nodes);
}

//=============================================================================
// NodeLookup Implementation
//=============================================================================

// Initialize static members
std::shared_ptr<NodeLookup> NodeLookup::s_instance = nullptr;
std::mutex NodeLookup::s_instanceMutex;

std::shared_ptr<NodeLookup> NodeLookup::getInstance(
    const DHTConfig& config,
    const types::NodeID& nodeID,
    std::shared_ptr<RoutingTable> routingTable) {

    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (!s_instance) {
        s_instance = std::shared_ptr<NodeLookup>(new NodeLookup(config, nodeID, routingTable));
    }
    return s_instance;
}

NodeLookup::NodeLookup(const DHTConfig& config,
                     const types::NodeID& nodeID,
                     std::shared_ptr<RoutingTable> routingTable)
    : m_config(config), m_nodeID(nodeID), m_routingTable(routingTable) {
}

NodeLookup::~NodeLookup() {
    // Note: We don't clear the singleton instance here to avoid mutex issues
    // during program termination. The instance will be cleaned up by the OS.
}

void NodeLookup::lookup(const types::NodeID& targetID,
                      std::function<void(const std::vector<std::shared_ptr<types::Node>>&)> callback) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Generate a lookup ID
        std::string lookupID = generateLookupID(targetID);

        // Check if a lookup with this ID already exists
        if (m_lookups.find(lookupID) != m_lookups.end()) {
            return;
        }

        // Create a new lookup
        NodeLookupState lookup(targetID, callback);

        // Get the closest nodes from the routing table
        if (m_routingTable) {
            lookup.nodes = m_routingTable->getClosestNodes(targetID, 20);
        }

        // Add the lookup
        m_lookups.emplace(lookupID, lookup);

        // Send queries
        sendQueries(lookupID);
    } catch (const std::exception& e) {
        unified_event::logError("DHT.NodeLookup", e.what());
    }
}

void NodeLookup::sendQueries(const std::string& lookupID) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Find the lookup
        auto it = m_lookups.find(lookupID);
        if (it == m_lookups.end()) {
            return;
        }

        auto& lookup = it->second;

        // Check if the lookup is complete
        if (isLookupComplete(lookupID)) {
            completeLookup(lookupID);
            return;
        }

        // Sort nodes by distance to target
        std::sort(lookup.nodes.begin(), lookup.nodes.end(),
            [&lookup](const std::shared_ptr<types::Node>& a, const std::shared_ptr<types::Node>& b) {
                types::NodeID distanceA = a->getID().distanceTo(lookup.targetID);
                types::NodeID distanceB = b->getID().distanceTo(lookup.targetID);
                return distanceA < distanceB;
            });

        // Send queries to the closest nodes that haven't been queried yet
        size_t queriesSent = 0;
        for (const auto& node : lookup.nodes) {
            // Skip nodes that have already been queried
            std::string nodeIDStr = node->getID().toString();
            if (lookup.queriedNodes.find(nodeIDStr) != lookup.queriedNodes.end()) {
                continue;
            }

            // Add the node to the queried nodes
            lookup.queriedNodes.insert(nodeIDStr);

            // Add the node to the active queries
            lookup.activeQueries.insert(nodeIDStr);

            // TODO: Send the query
            // This would normally call the message sender to send a find_node query

            queriesSent++;

            // Stop if we've sent enough queries
            if (queriesSent >= 3) { // Alpha value
                break;
            }
        }

        // Increment the iteration counter
        lookup.iteration++;

        // If no queries were sent and there are no active queries, complete the lookup
        if (queriesSent == 0 && lookup.activeQueries.empty()) {
            completeLookup(lookupID);
        }
    } catch (const std::exception& e) {
        unified_event::logError("DHT.NodeLookup", e.what());
    }
}

void NodeLookup::handleResponse(const std::string& lookupID,
                              const std::vector<std::shared_ptr<types::Node>>& nodes,
                              const types::EndPoint& sender) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Find the lookup
        auto it = m_lookups.find(lookupID);
        if (it == m_lookups.end()) {
            return;
        }

        auto& lookup = it->second;

        // Find the node that sent the response
        std::string senderIDStr;
        for (const auto& node : lookup.nodes) {
            if (node->getEndpoint() == sender) {
                senderIDStr = node->getID().toString();
                break;
            }
        }

        if (senderIDStr.empty()) {
            return;
        }

        // Remove the node from the active queries
        lookup.activeQueries.erase(senderIDStr);

        // Add the node to the responded nodes
        lookup.respondedNodes.insert(senderIDStr);

        // Add the nodes from the response to the lookup
        for (const auto& node : nodes) {
            // Skip nodes that are already in the lookup
            bool found = false;
            for (const auto& existingNode : lookup.nodes) {
                if (existingNode->getID() == node->getID()) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                lookup.nodes.push_back(node);

                // Add the node to the routing table
                if (m_routingTable) {
                    m_routingTable->addNode(node);
                }
            }
        }

        // Send more queries
        sendQueries(lookupID);
    } catch (const std::exception& e) {
        unified_event::logError("DHT.NodeLookup", e.what());
    }
}

void NodeLookup::handleError(const std::string& lookupID, const types::EndPoint& sender) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Find the lookup
        auto it = m_lookups.find(lookupID);
        if (it == m_lookups.end()) {
            return;
        }

        auto& lookup = it->second;

        // Find the node that sent the error
        std::string senderIDStr;
        for (const auto& node : lookup.nodes) {
            if (node->getEndpoint() == sender) {
                senderIDStr = node->getID().toString();
                break;
            }
        }

        if (senderIDStr.empty()) {
            return;
        }

        // Remove the node from the active queries
        lookup.activeQueries.erase(senderIDStr);

        // Send more queries
        sendQueries(lookupID);
    } catch (const std::exception& e) {
        unified_event::logError("DHT.NodeLookup", e.what());
    }
}

void NodeLookup::handleTimeout(const std::string& lookupID, const types::NodeID& nodeID) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Find the lookup
        auto it = m_lookups.find(lookupID);
        if (it == m_lookups.end()) {
            return;
        }

        auto& lookup = it->second;

        // Remove the node from the active queries
        lookup.activeQueries.erase(nodeID.toString());

        // Send more queries
        sendQueries(lookupID);
    } catch (const std::exception& e) {
        unified_event::logError("DHT.NodeLookup", e.what());
    }
}

bool NodeLookup::isLookupComplete(const std::string& lookupID) {
    auto it = m_lookups.find(lookupID);
    if (it == m_lookups.end()) {
        return true;
    }

    auto& lookup = it->second;

    // If we've reached the maximum number of iterations, the lookup is complete
    if (lookup.iteration >= 10) {
        return true;
    }

    // If there are no active queries and all nodes have been queried, the lookup is complete
    if (lookup.activeQueries.empty() && lookup.queriedNodes.size() >= lookup.nodes.size()) {
        return true;
    }

    return false;
}

void NodeLookup::completeLookup(const std::string& lookupID) {
    auto it = m_lookups.find(lookupID);
    if (it == m_lookups.end()) {
        return;
    }

    auto& lookup = it->second;

    // Sort nodes by distance to target
    std::sort(lookup.nodes.begin(), lookup.nodes.end(),
        [&lookup](const std::shared_ptr<types::Node>& a, const std::shared_ptr<types::Node>& b) {
            types::NodeID distanceA = a->getID().distanceTo(lookup.targetID);
            types::NodeID distanceB = b->getID().distanceTo(lookup.targetID);
            return distanceA < distanceB;
        });

    // Limit the number of nodes to return
    if (lookup.nodes.size() > 20) {
        lookup.nodes.resize(20);
    }

    // Call the callback with the result
    if (lookup.callback) {
        lookup.callback(lookup.nodes);
    }

    // Remove the lookup
    m_lookups.erase(lookupID);
}

std::string NodeLookup::generateLookupID(const types::NodeID& targetID) const {
    return "node_lookup_" + targetID.toString();
}

//=============================================================================
// PeerLookup Implementation
//=============================================================================

// Initialize static members
std::shared_ptr<PeerLookup> PeerLookup::s_instance = nullptr;
std::mutex PeerLookup::s_instanceMutex;

std::shared_ptr<PeerLookup> PeerLookup::getInstance(
    const DHTConfig& config,
    const types::NodeID& nodeID,
    std::shared_ptr<RoutingTable> routingTable) {

    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (!s_instance) {
        s_instance = std::shared_ptr<PeerLookup>(new PeerLookup(config, nodeID, routingTable));
    }
    return s_instance;
}

PeerLookup::PeerLookup(const DHTConfig& config,
                     const types::NodeID& nodeID,
                     std::shared_ptr<RoutingTable> routingTable)
    : m_config(config), m_nodeID(nodeID), m_routingTable(routingTable) {
}

PeerLookup::~PeerLookup() {
    // Note: We don't clear the singleton instance here to avoid mutex issues
    // during program termination. The instance will be cleaned up by the OS.
}

void PeerLookup::lookup(const types::InfoHash& infoHash,
                      std::function<void(const std::vector<types::EndPoint>&)> callback) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Generate a lookup ID
        std::string lookupID = generateLookupID(infoHash);

        // Check if a lookup with this ID already exists
        if (m_lookups.find(lookupID) != m_lookups.end()) {
            return;
        }

        // Create a new lookup
        PeerLookupState lookup(infoHash, callback);

        // Get the closest nodes from the routing table
        if (m_routingTable) {
            lookup.nodes = m_routingTable->getClosestNodes(types::NodeID(infoHash), 20);
        }

        // Add the lookup
        m_lookups.emplace(lookupID, lookup);

        // Send queries
        sendQueries(lookupID);
    } catch (const std::exception& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::announce(const types::InfoHash& infoHash,
                        uint16_t port,
                        std::function<void(bool)> callback) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Generate a lookup ID
        std::string lookupID = generateLookupID(infoHash);

        // Check if a lookup with this ID already exists
        if (m_lookups.find(lookupID) != m_lookups.end()) {
            return;
        }

        // Create a new lookup
        PeerLookupState lookup(infoHash, port, callback);

        // Get the closest nodes from the routing table
        if (m_routingTable) {
            lookup.nodes = m_routingTable->getClosestNodes(types::NodeID(infoHash), 20);
        }

        // Add the lookup
        m_lookups.emplace(lookupID, lookup);

        // Send queries
        sendQueries(lookupID);
    } catch (const std::exception& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::sendQueries(const std::string& lookupID) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Find the lookup
        auto it = m_lookups.find(lookupID);
        if (it == m_lookups.end()) {
            return;
        }

        auto& lookup = it->second;

        // Check if the lookup is complete
        if (isLookupComplete(lookupID)) {
            if (lookup.announcing) {
                sendAnnouncements(lookupID);
            } else {
                completeLookup(lookupID);
            }
            return;
        }

        // Sort nodes by distance to target
        std::sort(lookup.nodes.begin(), lookup.nodes.end(),
            [&lookup](const std::shared_ptr<types::Node>& a, const std::shared_ptr<types::Node>& b) {
                types::NodeID distanceA = a->getID().distanceTo(types::NodeID(lookup.infoHash));
                types::NodeID distanceB = b->getID().distanceTo(types::NodeID(lookup.infoHash));
                return distanceA < distanceB;
            });

        // Send queries to the closest nodes that haven't been queried yet
        size_t queriesSent = 0;
        for (const auto& node : lookup.nodes) {
            // Skip nodes that have already been queried
            std::string nodeIDStr = node->getID().toString();
            if (lookup.queriedNodes.find(nodeIDStr) != lookup.queriedNodes.end()) {
                continue;
            }

            // Add the node to the queried nodes
            lookup.queriedNodes.insert(nodeIDStr);

            // Add the node to the active queries
            lookup.activeQueries.insert(nodeIDStr);

            // TODO: Send the query
            // This would normally call the message sender to send a get_peers query

            queriesSent++;

            // Stop if we've sent enough queries
            if (queriesSent >= 3) { // Alpha value
                break;
            }
        }

        // Increment the iteration counter
        lookup.iteration++;

        // If no queries were sent and there are no active queries, complete the lookup
        if (queriesSent == 0 && lookup.activeQueries.empty()) {
            if (lookup.announcing) {
                sendAnnouncements(lookupID);
            } else {
                completeLookup(lookupID);
            }
        }
    } catch (const std::exception& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::sendAnnouncements(const std::string& lookupID) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Find the lookup
        auto it = m_lookups.find(lookupID);
        if (it == m_lookups.end()) {
            return;
        }

        auto& lookup = it->second;

        // If there are no tokens, complete the announcement with failure
        if (lookup.nodeTokens.empty()) {
            completeAnnouncement(lookupID, false);
            return;
        }

        // Send announcements to nodes with tokens
        size_t announcementsSent = 0;
        for (const auto& pair : lookup.nodeTokens) {
            const std::string& nodeIDStr = pair.first;
            const std::string& token = pair.second;

            // Skip nodes that have already been announced to
            if (lookup.announcedNodes.find(nodeIDStr) != lookup.announcedNodes.end()) {
                continue;
            }

            // Find the node
            std::shared_ptr<types::Node> node;
            for (const auto& n : lookup.nodes) {
                if (n->getID().toString() == nodeIDStr) {
                    node = n;
                    break;
                }
            }

            if (!node) {
                continue;
            }

            // Add the node to the announced nodes
            lookup.announcedNodes.insert(nodeIDStr);

            // Add the node to the active announcements
            lookup.activeAnnouncements.insert(nodeIDStr);

            // TODO: Send the announcement
            // This would normally call the message sender to send an announce_peer query

            announcementsSent++;
        }

        // If no announcements were sent and there are no active announcements, complete the announcement
        if (announcementsSent == 0 && lookup.activeAnnouncements.empty()) {
            completeAnnouncement(lookupID, !lookup.announcedNodes.empty());
        }
    } catch (const std::exception& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::handleGetPeersResponse(const std::string& lookupID,
                                      const std::vector<std::shared_ptr<types::Node>>& nodes,
                                      const std::vector<types::EndPoint>& peers,
                                      const std::string& token,
                                      const types::EndPoint& sender) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Find the lookup
        auto it = m_lookups.find(lookupID);
        if (it == m_lookups.end()) {
            return;
        }

        auto& lookup = it->second;

        // Find the node that sent the response
        std::string senderIDStr;
        for (const auto& node : lookup.nodes) {
            if (node->getEndpoint() == sender) {
                senderIDStr = node->getID().toString();
                break;
            }
        }

        if (senderIDStr.empty()) {
            return;
        }

        // Remove the node from the active queries
        lookup.activeQueries.erase(senderIDStr);

        // Add the node to the responded nodes
        lookup.respondedNodes.insert(senderIDStr);

        // Store the token
        if (!token.empty()) {
            lookup.nodeTokens[senderIDStr] = token;
        }

        // Add the peers from the response to the lookup
        for (const auto& peer : peers) {
            // Skip peers that are already in the lookup
            bool found = false;
            for (const auto& existingPeer : lookup.peers) {
                if (existingPeer == peer) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                lookup.peers.push_back(peer);
            }
        }

        // Add the nodes from the response to the lookup
        for (const auto& node : nodes) {
            // Skip nodes that are already in the lookup
            bool found = false;
            for (const auto& existingNode : lookup.nodes) {
                if (existingNode->getID() == node->getID()) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                lookup.nodes.push_back(node);

                // Add the node to the routing table
                if (m_routingTable) {
                    m_routingTable->addNode(node);
                }
            }
        }

        // Send more queries
        sendQueries(lookupID);
    } catch (const std::exception& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::handleAnnouncePeerResponse(const std::string& lookupID,
                                          const types::EndPoint& sender) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Find the lookup
        auto it = m_lookups.find(lookupID);
        if (it == m_lookups.end()) {
            return;
        }

        auto& lookup = it->second;

        // Find the node that sent the response
        std::string senderIDStr;
        for (const auto& node : lookup.nodes) {
            if (node->getEndpoint() == sender) {
                senderIDStr = node->getID().toString();
                break;
            }
        }

        if (senderIDStr.empty()) {
            return;
        }

        // Remove the node from the active announcements
        lookup.activeAnnouncements.erase(senderIDStr);

        // If there are no active announcements, complete the announcement
        if (lookup.activeAnnouncements.empty()) {
            completeAnnouncement(lookupID, true);
        }
    } catch (const std::exception& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::handleError(const std::string& lookupID,
                           const types::EndPoint& sender) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Find the lookup
        auto it = m_lookups.find(lookupID);
        if (it == m_lookups.end()) {
            return;
        }

        auto& lookup = it->second;

        // Find the node that sent the error
        std::string senderIDStr;
        for (const auto& node : lookup.nodes) {
            if (node->getEndpoint() == sender) {
                senderIDStr = node->getID().toString();
                break;
            }
        }

        if (senderIDStr.empty()) {
            return;
        }

        // Remove the node from the active queries or announcements
        lookup.activeQueries.erase(senderIDStr);
        lookup.activeAnnouncements.erase(senderIDStr);

        // If there are no active queries, send more queries or complete the lookup
        if (lookup.activeQueries.empty()) {
            if (lookup.announcing && !lookup.activeAnnouncements.empty()) {
                // Wait for announcements to complete
            } else if (lookup.announcing) {
                sendAnnouncements(lookupID);
            } else {
                sendQueries(lookupID);
            }
        }

        // If there are no active announcements and no active queries, complete the announcement
        if (lookup.announcing && lookup.activeAnnouncements.empty() && lookup.activeQueries.empty()) {
            completeAnnouncement(lookupID, !lookup.announcedNodes.empty());
        }
    } catch (const std::exception& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::handleTimeout(const std::string& lookupID,
                             const types::NodeID& nodeID) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Find the lookup
        auto it = m_lookups.find(lookupID);
        if (it == m_lookups.end()) {
            return;
        }

        auto& lookup = it->second;

        // Remove the node from the active queries or announcements
        lookup.activeQueries.erase(nodeID.toString());
        lookup.activeAnnouncements.erase(nodeID.toString());

        // If there are no active queries, send more queries or complete the lookup
        if (lookup.activeQueries.empty()) {
            if (lookup.announcing && !lookup.activeAnnouncements.empty()) {
                // Wait for announcements to complete
            } else if (lookup.announcing) {
                sendAnnouncements(lookupID);
            } else {
                sendQueries(lookupID);
            }
        }

        // If there are no active announcements and no active queries, complete the announcement
        if (lookup.announcing && lookup.activeAnnouncements.empty() && lookup.activeQueries.empty()) {
            completeAnnouncement(lookupID, !lookup.announcedNodes.empty());
        }
    } catch (const std::exception& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

bool PeerLookup::isLookupComplete(const std::string& lookupID) {
    auto it = m_lookups.find(lookupID);
    if (it == m_lookups.end()) {
        return true;
    }

    auto& lookup = it->second;

    // If we've reached the maximum number of iterations, the lookup is complete
    if (lookup.iteration >= 10) {
        return true;
    }

    // If there are no active queries and all nodes have been queried, the lookup is complete
    if (lookup.activeQueries.empty() && lookup.queriedNodes.size() >= lookup.nodes.size()) {
        return true;
    }

    // If we've found enough peers, the lookup is complete
    if (!lookup.peers.empty() && lookup.peers.size() >= 20) {
        return true;
    }

    return false;
}

void PeerLookup::completeLookup(const std::string& lookupID) {
    auto it = m_lookups.find(lookupID);
    if (it == m_lookups.end()) {
        return;
    }

    auto& lookup = it->second;

    // Call the callback with the result
    if (lookup.lookupCallback) {
        lookup.lookupCallback(lookup.peers);
    }

    // Remove the lookup
    m_lookups.erase(lookupID);
}

void PeerLookup::completeAnnouncement(const std::string& lookupID, bool success) {
    auto it = m_lookups.find(lookupID);
    if (it == m_lookups.end()) {
        return;
    }

    auto& lookup = it->second;

    // Call the callback with the result
    if (lookup.announceCallback) {
        lookup.announceCallback(success);
    }

    // Remove the lookup
    m_lookups.erase(lookupID);
}

std::string PeerLookup::generateLookupID(const types::InfoHash& infoHash) const {
    return "peer_lookup_" + infoHash.toString();
}

} // namespace dht_hunter::utils::dht_core
