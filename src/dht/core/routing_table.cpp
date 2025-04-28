#include "dht_hunter/dht/core/routing_table.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"
#include "dht_hunter/unified_event/events/node_events.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace dht_hunter::dht {

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
        // Lock both mutexes to prevent data races
        // Note: We need to be careful about the order of lock acquisition to prevent deadlocks
        // We'll use a simple address-based ordering to ensure consistent lock acquisition order
        if (this < &other) {
            try {
                utility::thread::withLock(m_mutex, [this, &other]() {
                    try {
                        utility::thread::withLock(other.m_mutex, [this, &other]() {
                            // Both locks acquired, now do the move
                            m_prefix = other.m_prefix;
                            m_kSize = other.m_kSize;
                            m_nodes = std::move(other.m_nodes);
                            m_lastChanged = other.m_lastChanged;
                        }, "KBucket::other.m_mutex");
                    } catch (const utility::thread::LockTimeoutException& e) {
                        // Log error and rethrow
                        unified_event::logError("DHT.RoutingTable", e.what());
                        throw;
                    }
                }, "KBucket::m_mutex");
            } catch (const utility::thread::LockTimeoutException& e) {
                unified_event::logError("DHT.RoutingTable", e.what());
                // Continue without moving, better than deadlocking
            }
        } else {
            try {
                utility::thread::withLock(other.m_mutex, [this, &other]() {
                    try {
                        utility::thread::withLock(m_mutex, [this, &other]() {
                            // Both locks acquired, now do the move
                            m_prefix = other.m_prefix;
                            m_kSize = other.m_kSize;
                            m_nodes = std::move(other.m_nodes);
                            m_lastChanged = other.m_lastChanged;
                        }, "KBucket::m_mutex");
                    } catch (const utility::thread::LockTimeoutException& e) {
                        // Log error and rethrow
                        unified_event::logError("DHT.RoutingTable", e.what());
                        throw;
                    }
                }, "KBucket::other.m_mutex");
            } catch (const utility::thread::LockTimeoutException& e) {
                unified_event::logError("DHT.RoutingTable", e.what());
                // Continue without moving, better than deadlocking
            }
        }

        // The move is now handled in the lock acquisition code above
    }
    return *this;
}

bool KBucket::addNode(std::shared_ptr<Node> node) {
    if (!node) {
        return false;
    }

    try {
        return utility::thread::withLock(m_mutex, [this, node]() {
            // Check if the node is already in the bucket
            auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
                [&node](const std::shared_ptr<Node>& existingNode) {
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

            // Bucket is full, can't add the node
            return false;
        }, "KBucket::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return false;
    }
}

bool KBucket::removeNode(const NodeID& nodeID) {
    try {
        return utility::thread::withLock(m_mutex, [this, &nodeID]() {
            auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
                [&nodeID](const std::shared_ptr<Node>& node) {
                    return node->getID() == nodeID;
                });

            if (it != m_nodes.end()) {
                m_nodes.erase(it);
                // Update the last changed time
                updateLastChanged();
                return true;
            }

            return false;
        }, "KBucket::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return false;
    }
}

std::shared_ptr<Node> KBucket::getNode(const NodeID& nodeID) const {
    try {
        return utility::thread::withLock(m_mutex, [this, &nodeID]() {
            auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
                [&nodeID](const std::shared_ptr<Node>& node) {
                    return node->getID() == nodeID;
                });

            if (it != m_nodes.end()) {
                return *it;
            }

            return std::shared_ptr<Node>(nullptr);
        }, "KBucket::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return nullptr;
    }
}

std::vector<std::shared_ptr<Node>> KBucket::getNodes() const {
    try {
        return utility::thread::withLock(m_mutex, [this]() {
            return m_nodes;
        }, "KBucket::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return std::vector<std::shared_ptr<Node>>();
    }
}

size_t KBucket::getNodeCount() const {
    try {
        return utility::thread::withLock(m_mutex, [this]() {
            return m_nodes.size();
        }, "KBucket::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return 0;
    }
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

std::chrono::steady_clock::time_point KBucket::getLastChanged() const {
    try {
        return utility::thread::withLock(m_mutex, [this]() {
            return m_lastChanged;
        }, "KBucket::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return std::chrono::steady_clock::now(); // Return current time as fallback
    }
}

void KBucket::updateLastChanged() {
    m_lastChanged = std::chrono::steady_clock::now();
}

// Initialize static members
std::shared_ptr<RoutingTable> RoutingTable::s_instance = nullptr;
std::mutex RoutingTable::s_instanceMutex;

std::shared_ptr<RoutingTable> RoutingTable::getInstance(const NodeID& ownID, size_t kBucketSize) {
    try {
        return utility::thread::withLock(s_instanceMutex, [&ownID, kBucketSize]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<RoutingTable>(new RoutingTable(ownID, kBucketSize));
            }
            return s_instance;
        }, "RoutingTable::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return nullptr;
    }
}

RoutingTable::RoutingTable(const NodeID& ownID, size_t kBucketSize)
    : m_ownID(ownID), m_kBucketSize(kBucketSize), m_eventBus(unified_event::EventBus::getInstance()) {
    // Start with a single bucket with prefix 0
    m_buckets.emplace_back(0, kBucketSize);
}

RoutingTable::~RoutingTable() {
    // Clear the singleton instance
    try {
        utility::thread::withLock(s_instanceMutex, [this]() {
            if (s_instance.get() == this) {
                s_instance.reset();
            }
        }, "RoutingTable::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
    }
}

bool RoutingTable::addNode(const std::shared_ptr<Node> &node) {
    if (!node || !node->getID().isValid() || node->getID() == m_ownID) {
        return false;
    }

    try {
        return utility::thread::withLock(m_mutex, [this, &node]() {
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
        }, "RoutingTable::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return false;
    }
}

bool RoutingTable::removeNode(const NodeID& nodeID) {
    try {
        return utility::thread::withLock(m_mutex, [this, &nodeID]() {
            // Find the appropriate bucket
            KBucket& bucket = const_cast<KBucket&>(getBucketForNodeID(nodeID));

            // Try to remove the node from the bucket
            if (bucket.removeNode(nodeID)) {
                return true;
            }

            return false;
        }, "RoutingTable::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return false;
    }
}

std::shared_ptr<Node> RoutingTable::getNode(const NodeID& nodeID) const {
    try {
        return utility::thread::withLock(m_mutex, [this, &nodeID]() {
            // Find the appropriate bucket
            const KBucket& bucket = getBucketForNodeID(nodeID);

            // Try to get the node from the bucket
            return bucket.getNode(nodeID);
        }, "RoutingTable::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return nullptr;
    }
}

std::vector<std::shared_ptr<Node>> RoutingTable::getClosestNodes(const NodeID& targetID, size_t k) const {
    try {
        return utility::thread::withLock(m_mutex, [this, &targetID, k]() {
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
        }, "RoutingTable::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return std::vector<std::shared_ptr<Node>>();
    }
}

std::vector<std::shared_ptr<Node>> RoutingTable::getAllNodes() const {
    try {
        return utility::thread::withLock(m_mutex, [this]() {
            std::vector<std::shared_ptr<Node>> allNodes;
            for (const auto& bucket : m_buckets) {
                auto bucketNodes = bucket.getNodes();
                allNodes.insert(allNodes.end(), bucketNodes.begin(), bucketNodes.end());
            }

            return allNodes;
        }, "RoutingTable::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return std::vector<std::shared_ptr<Node>>();
    }
}

size_t RoutingTable::getNodeCount() const {
    try {
        return utility::thread::withLock(m_mutex, [this]() {
            size_t count = 0;
            for (const auto& bucket : m_buckets) {
                count += bucket.getNodeCount();
            }

            return count;
        }, "RoutingTable::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return 0;
    }
}

bool RoutingTable::saveToFile(const std::string& filePath) const {
    try {
        return utility::thread::withLock(m_mutex, [this, &filePath]() {
            std::ofstream file(filePath, std::ios::binary);
            if (!file) {
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
                    auto addressLength = static_cast<uint16_t>(address.length());
                    file.write(reinterpret_cast<const char*>(&addressLength), sizeof(addressLength));
                    file.write(address.c_str(), addressLength);

                    // Write the port
                    file.write(reinterpret_cast<const char*>(&port), sizeof(port));
                }
            }
            return true;
        }, "RoutingTable::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return false;
    }
}

bool RoutingTable::loadFromFile(const std::string& filePath) {
    try {
        return utility::thread::withLock(m_mutex, [this, &filePath]() {
            std::ifstream file(filePath, std::ios::binary);
            if (!file) {
                return false;
            }

            // Clear the routing table
            m_buckets.clear();
            m_buckets.emplace_back(0, m_kBucketSize);

            // Read the number of nodes
            size_t nodeCount = 0;
            file.read(reinterpret_cast<char*>(&nodeCount), sizeof(nodeCount));

            if (nodeCount == 0) {
                return true;
            }

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
            return true;
        }, "RoutingTable::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return false;
    }
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
}

bool RoutingTable::needsRefresh(size_t bucketIndex) const {
    try {
        return utility::thread::withLock(m_mutex, [this, bucketIndex]() {
            if (bucketIndex >= m_buckets.size()) {
                return false;
            }

            // According to BEP-5, buckets that have not been changed in 15 minutes should be refreshed
            auto now = std::chrono::steady_clock::now();
            auto lastChanged = m_buckets[bucketIndex].getLastChanged();
            auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - lastChanged).count();

            return elapsed >= 15; // 15 minutes as specified in BEP-5
        }, "RoutingTable::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return false; // Assume no refresh needed on error
    }
}

void RoutingTable::refreshBucket(size_t bucketIndex, std::function<void(const std::vector<std::shared_ptr<Node>>&)> callback) {
    try {
        utility::thread::withLock(m_mutex, [this, bucketIndex, &callback]() {
            if (bucketIndex >= m_buckets.size()) {
                if (callback) {
                    callback({});
                }
                return;
            }
        }, "RoutingTable::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        if (callback) {
            callback({});
        }
        return;
    }

    // Generate a random ID in the range of the bucket
    NodeID randomID = generateRandomIDInBucket(bucketIndex);

    // Update the last changed time for the bucket
    m_buckets[bucketIndex].updateLastChanged();

    // The lock is automatically released when the withLock lambda returns

    // Publish a bucket refresh event
    if (m_eventBus) {
        auto event = std::make_shared<unified_event::BucketRefreshEvent>(bucketIndex, randomID.toString());
        m_eventBus->publish(event);
    }

    // The actual lookup will be performed by the RoutingManager or NodeLookup
    // We need to pass the callback to them so they can call it when the lookup is complete
    // Since we don't have direct access to those components here, we'll rely on the caller
    // to handle the actual lookup
    if (callback) {
        // For now, we'll just return the nodes currently in the bucket
        // The caller should use a NodeLookup to find more nodes
        std::vector<std::shared_ptr<Node>> nodesInBucket;

        // Re-acquire the lock to access the bucket
        try {
            utility::thread::withLock(m_mutex, [this, bucketIndex, &nodesInBucket]() {
                if (bucketIndex < m_buckets.size()) {
                    nodesInBucket = m_buckets[bucketIndex].getNodes();
                }
            }, "RoutingTable::m_mutex");
        } catch (const utility::thread::LockTimeoutException& e) {
            unified_event::logError("DHT.RoutingTable", e.what());
        }

        callback(nodesInBucket);
    }
}

size_t RoutingTable::getBucketCount() const {
    try {
        return utility::thread::withLock(m_mutex, [this]() {
            return m_buckets.size();
        }, "RoutingTable::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return 0; // Return 0 as fallback
    }
}

size_t RoutingTable::getBucketIndex(const NodeID& nodeID) const {
    try {
        return utility::thread::withLock(m_mutex, [this, &nodeID]() {
            // Calculate the distance between our node ID and the given node ID
            NodeID distance = m_ownID.distanceTo(nodeID);

            // Find the index of the first bit that is 1 in the distance
            // This is the bucket index
            size_t bucketIndex = 0;
            // NodeID is a 20-byte array
            for (size_t i = 0; i < 20; ++i) {
                for (size_t j = 7; j >= 0 && j < 8; --j) { // Ensure j is unsigned and doesn't underflow
                    if ((distance[i] & (1 << j)) != 0) {
                        // Found a 1 bit, this is the bucket index
                        bucketIndex = (i * 8) + (7 - j);
                        return std::min(bucketIndex, m_buckets.size() - 1);
                    }
                }
            }

            // If no 1 bit is found, return the last bucket
            return m_buckets.size() - 1;
        }, "RoutingTable::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.RoutingTable", e.what());
        return 0; // Return 0 as fallback
    }
}

NodeID RoutingTable::generateRandomIDInBucket(size_t bucketIndex) const {
    // Generate a random ID in the range of the bucket
    // The bucket covers a specific prefix of the ID space
    NodeID randomID = NodeID::random();

    // Set the prefix bits to match the bucket's prefix
    size_t prefix = m_buckets[bucketIndex].getPrefix();
    size_t prefixBits = prefix;

    // Set the prefix bits of the random ID to match the bucket's prefix
    for (size_t i = 0; i < prefixBits; ++i) {
        size_t byteIndex = i / 8;
        size_t bitIndex = 7 - (i % 8);

        // Get the bit from the node ID
        bool bit = (m_ownID[byteIndex] & (1 << bitIndex)) != 0;

        // Set the bit in the random ID
        if (bit) {
            randomID[byteIndex] |= (1 << bitIndex);
        } else {
            randomID[byteIndex] &= ~(1 << bitIndex);
        }
    }

    return randomID;
}

} // namespace dht_hunter::dht
