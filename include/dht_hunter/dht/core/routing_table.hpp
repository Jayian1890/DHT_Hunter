#pragma once

#include "dht_hunter/dht/types/dht_types.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/event/logger.hpp"
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <chrono>

namespace dht_hunter::dht {

/**
 * @brief A k-bucket in the routing table
 */
class KBucket {
public:
    /**
     * @brief Constructs a k-bucket
     * @param prefix The prefix length of the bucket (in bits)
     * @param kSize The maximum number of nodes in the bucket
     */
    KBucket(size_t prefix, size_t kSize);

    /**
     * @brief Move constructor
     * @param other The k-bucket to move from
     */
    KBucket(KBucket&& other) noexcept;

    /**
     * @brief Move assignment operator
     * @param other The k-bucket to move from
     * @return Reference to this k-bucket
     */
    KBucket& operator=(KBucket&& other) noexcept;

    /**
     * @brief Copy constructor (deleted)
     */
    KBucket(const KBucket&) = delete;

    /**
     * @brief Copy assignment operator (deleted)
     */
    KBucket& operator=(const KBucket&) = delete;

    /**
     * @brief Adds a node to the bucket
     * @param node The node to add
     * @return True if the node was added, false otherwise
     */
    bool addNode(std::shared_ptr<Node> node);

    /**
     * @brief Removes a node from the bucket
     * @param nodeID The ID of the node to remove
     * @return True if the node was removed, false otherwise
     */
    bool removeNode(const NodeID& nodeID);

    /**
     * @brief Gets a node from the bucket
     * @param nodeID The ID of the node to get
     * @return The node, or nullptr if not found
     */
    std::shared_ptr<Node> getNode(const NodeID& nodeID) const;

    /**
     * @brief Gets all nodes in the bucket
     * @return The nodes in the bucket
     */
    std::vector<std::shared_ptr<Node>> getNodes() const;

    /**
     * @brief Gets the number of nodes in the bucket
     * @return The number of nodes in the bucket
     */
    size_t getNodeCount() const;

    /**
     * @brief Checks if a node ID belongs in this bucket
     * @param nodeID The node ID to check
     * @param ownID The node ID of the local node
     * @return True if the node ID belongs in this bucket, false otherwise
     */
    bool containsNodeID(const NodeID& nodeID, const NodeID& ownID) const;

    /**
     * @brief Gets the prefix length of the bucket
     * @return The prefix length in bits
     */
    size_t getPrefix() const;

    /**
     * @brief Gets the last time the bucket was changed
     * @return The last time the bucket was changed
     */
    std::chrono::steady_clock::time_point getLastChanged() const;

    /**
     * @brief Updates the last changed time to now
     */
    void updateLastChanged();

private:
    size_t m_prefix;
    size_t m_kSize;
    std::vector<std::shared_ptr<Node>> m_nodes;
    std::chrono::steady_clock::time_point m_lastChanged; // Time when the bucket was last changed
    mutable std::mutex m_mutex;
};

/**
 * @brief A routing table for the DHT (Singleton)
 */
class RoutingTable {
public:
    /**
     * @brief Gets the singleton instance of the routing table
     * @param ownID The ID of the node that owns this routing table (only used if instance doesn't exist yet)
     * @param kBucketSize The maximum number of nodes in each k-bucket (only used if instance doesn't exist yet)
     * @return The singleton instance
     */
    static std::shared_ptr<RoutingTable> getInstance(const NodeID& ownID, size_t kBucketSize = K_BUCKET_SIZE);

    /**
     * @brief Destructor
     */
    ~RoutingTable();

    /**
     * @brief Delete copy constructor and assignment operator
     */
    RoutingTable(const RoutingTable&) = delete;
    RoutingTable& operator=(const RoutingTable&) = delete;
    RoutingTable(RoutingTable&&) = delete;
    RoutingTable& operator=(RoutingTable&&) = delete;

    /**
     * @brief Adds a node to the routing table
     * @param node The node to add
     * @return True if the node was added, false otherwise
     */
    bool addNode(const std::shared_ptr<Node> &node);

    /**
     * @brief Removes a node from the routing table
     * @param nodeID The ID of the node to remove
     * @return True if the node was removed, false otherwise
     */
    bool removeNode(const NodeID& nodeID);

    /**
     * @brief Gets a node from the routing table
     * @param nodeID The ID of the node to get
     * @return The node, or nullptr if not found
     */
    std::shared_ptr<Node> getNode(const NodeID& nodeID) const;

    /**
     * @brief Gets the k closest nodes to a target ID
     * @param targetID The target ID
     * @param k The maximum number of nodes to return
     * @return The k closest nodes
     */
    std::vector<std::shared_ptr<Node>> getClosestNodes(const NodeID& targetID, size_t k) const;

    /**
     * @brief Gets all nodes in the routing table
     * @return All nodes in the routing table
     */
    std::vector<std::shared_ptr<Node>> getAllNodes() const;

    /**
     * @brief Gets the number of nodes in the routing table
     * @return The number of nodes in the routing table
     */
    size_t getNodeCount() const;

    /**
     * @brief Gets the number of buckets in the routing table
     * @return The number of buckets
     */
    size_t getBucketCount() const;

    /**
     * @brief Saves the routing table to a file
     * @param filePath The path to the file
     * @return True if the routing table was saved successfully, false otherwise
     */
    bool saveToFile(const std::string& filePath) const;

    /**
     * @brief Loads the routing table from a file
     * @param filePath The path to the file
     * @return True if the routing table was loaded successfully, false otherwise
     */
    bool loadFromFile(const std::string& filePath);

    /**
     * @brief Checks if a bucket needs refreshing
     * @param bucketIndex The index of the bucket to check
     * @return True if the bucket needs refreshing, false otherwise
     */
    bool needsRefresh(size_t bucketIndex) const;

    /**
     * @brief Refreshes a bucket by performing a find_node lookup for a random ID in the bucket
     * @param bucketIndex The index of the bucket to refresh
     * @param callback The callback to call when the refresh is complete
     */
    void refreshBucket(size_t bucketIndex, std::function<void(const std::vector<std::shared_ptr<Node>>&)> callback = nullptr);

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    RoutingTable(const NodeID& ownID, size_t kBucketSize = K_BUCKET_SIZE);

    // Static instance for singleton pattern
    static std::shared_ptr<RoutingTable> s_instance;
    static std::mutex s_instanceMutex;

    /**
     * @brief Gets the appropriate k-bucket for a node ID
     * @param nodeID The node ID
     * @return The k-bucket
     */
    KBucket& getBucketForNodeID(const NodeID& nodeID);

    /**
     * @brief Gets the appropriate k-bucket for a node ID (const version)
     * @param nodeID The node ID
     * @return The k-bucket
     */
    const KBucket& getBucketForNodeID(const NodeID& nodeID) const;

    /**
     * @brief Splits a k-bucket
     * @param bucketIndex The index of the bucket to split
     */
    void splitBucket(size_t bucketIndex);

    /**
     * @brief Generates a random ID in the range of a bucket
     * @param bucketIndex The index of the bucket
     * @return A random ID in the range of the bucket
     */
    NodeID generateRandomIDInBucket(size_t bucketIndex) const;

    NodeID m_ownID;
    size_t m_kBucketSize;
    std::vector<KBucket> m_buckets;
    mutable std::mutex m_mutex;
    event::Logger m_logger;
};

} // namespace dht_hunter::dht
