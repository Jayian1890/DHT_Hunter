#pragma once

#include "dht_hunter/dht/core/dht_types.hpp"
#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/util/mutex_utils.hpp"
#include <memory>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>

namespace dht_hunter::dht {

/**
 * @brief A k-bucket in the routing table
 */
class KBucket {
public:
    /**
     * @brief Constructs a k-bucket
     * @param minDistance The minimum distance for nodes in this bucket
     * @param maxDistance The maximum distance for nodes in this bucket
     * @param kSize The maximum number of nodes in this bucket
     */
    KBucket(const NodeID& minDistance, const NodeID& maxDistance, size_t kSize);

    /**
     * @brief Move constructor
     * @param other The bucket to move from
     */
    KBucket(KBucket&& other) noexcept;

    /**
     * @brief Move assignment operator
     * @param other The bucket to move from
     * @return Reference to this bucket
     */
    KBucket& operator=(KBucket&& other) noexcept;

    /**
     * @brief Deleted copy constructor
     */
    KBucket(const KBucket&) = delete;

    /**
     * @brief Deleted copy assignment operator
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
     * @brief Finds a node in the bucket
     * @param nodeID The ID of the node to find
     * @return The node, or nullptr if not found
     */
    std::shared_ptr<Node> findNode(const NodeID& nodeID) const;

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
     * @brief Checks if a node ID is in the range of this bucket
     * @param nodeID The node ID to check
     * @return True if the node ID is in the range of this bucket, false otherwise
     */
    bool isInRange(const NodeID& nodeID) const;

private:
    NodeID m_minDistance;
    NodeID m_maxDistance;
    size_t m_kSize;
    std::unordered_map<std::string, std::shared_ptr<Node>> m_nodes;
    mutable util::CheckedMutex m_mutex;
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
    RoutingTable(const NodeID& ownID, size_t kBucketSize);

    /**
     * @brief Adds a node to the routing table
     * @param nodeID The ID of the node to add
     * @param endpoint The endpoint of the node to add
     * @return True if the node was added, false otherwise
     */
    bool addNode(const NodeID& nodeID, const network::EndPoint& endpoint);

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
     * @brief Finds a node in the routing table
     * @param nodeID The ID of the node to find
     * @return The node, or nullptr if not found
     */
    std::shared_ptr<Node> findNode(const NodeID& nodeID) const;

    /**
     * @brief Gets the closest nodes to a target ID
     * @param targetID The target ID
     * @param count The maximum number of nodes to return
     * @return The closest nodes to the target ID
     */
    std::vector<std::shared_ptr<Node>> getClosestNodes(const NodeID& targetID, size_t count) const;

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
    NodeID m_ownID;
    size_t m_kBucketSize;
    std::vector<KBucket> m_kBuckets;
    mutable util::CheckedMutex m_mutex;
};

} // namespace dht_hunter::dht
