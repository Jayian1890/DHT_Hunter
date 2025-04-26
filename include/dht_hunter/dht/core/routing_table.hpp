#pragma once

#include "dht_hunter/dht/core/dht_types.hpp"
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

private:
    size_t m_prefix;
    size_t m_kSize;
    std::vector<std::shared_ptr<Node>> m_nodes;
    mutable std::mutex m_mutex;
};

/**
 * @brief A routing table for the DHT
 */
class RoutingTable {
public:
    /**
     * @brief Constructs a routing table
     * @param ownID The ID of the node that owns this routing table
     * @param kBucketSize The maximum number of nodes in each k-bucket
     */
    RoutingTable(const NodeID& ownID, size_t kBucketSize = K_BUCKET_SIZE);

    /**
     * @brief Adds a node to the routing table
     * @param node The node to add
     * @return True if the node was added, false otherwise
     */
    bool addNode(std::shared_ptr<Node> node);

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

private:
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

    NodeID m_ownID;
    size_t m_kBucketSize;
    std::vector<KBucket> m_buckets;
    mutable std::mutex m_mutex;
    event::Logger m_logger;
};

} // namespace dht_hunter::dht
