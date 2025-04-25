#pragma once

#include "dht_hunter/dht/types.hpp"
#include "dht_hunter/network/network_address.hpp"

#include <vector>
#include <list>
#include <memory>
#include <mutex>
#include <chrono>
#include <functional>
#include "dht_hunter/util/mutex_utils.hpp"

namespace dht_hunter::dht {

/**
 * @brief Default maximum number of nodes in a k-bucket
 * Increased from 8 to 16 for better performance and network coverage
 */
constexpr size_t DEFAULT_K_BUCKET_SIZE = 16;

/**
 * @brief Number of bits in a node ID
 */
constexpr size_t NODE_ID_BITS = 160;

/**
 * @brief Represents a node in the DHT network
 */
class Node {
public:
    /**
     * @brief Constructs a node
     * @param id The node ID
     * @param endpoint The node's endpoint
     */
    Node(const NodeID& id, const network::EndPoint& endpoint);

    /**
     * @brief Gets the node ID
     * @return The node ID
     */
    const NodeID& getID() const;

    /**
     * @brief Gets the node's endpoint
     * @return The node's endpoint
     */
    const network::EndPoint& getEndpoint() const;

    /**
     * @brief Gets the last time the node was seen
     * @return The last time the node was seen
     */
    std::chrono::steady_clock::time_point getLastSeen() const;

    /**
     * @brief Updates the last seen time to now
     */
    void updateLastSeen();

    /**
     * @brief Checks if the node is good (responsive)
     * @return True if the node is good, false otherwise
     */
    bool isGood() const;

    /**
     * @brief Sets whether the node is good (responsive)
     * @param good True if the node is good, false otherwise
     */
    void setGood(bool good);

    /**
     * @brief Gets the number of failed queries to this node
     * @return The number of failed queries
     */
    int getFailedQueries() const;

    /**
     * @brief Increments the number of failed queries to this node
     * @return The new number of failed queries
     */
    int incrementFailedQueries();

    /**
     * @brief Resets the number of failed queries to this node
     */
    void resetFailedQueries();

private:
    NodeID m_id;
    network::EndPoint m_endpoint;
    std::chrono::steady_clock::time_point m_lastSeen;
    bool m_isGood;
    int m_failedQueries;
};

/**
 * @brief Calculates the XOR distance between two node IDs
 * @param id1 The first node ID
 * @param id2 The second node ID
 * @return The XOR distance
 */
NodeID calculateDistance(const NodeID& id1, const NodeID& id2);

/**
 * @brief Gets the bucket index for a given distance
 * @param distance The distance
 * @return The bucket index
 */
int getBucketIndex(const NodeID& distance);

/**
 * @brief Represents a k-bucket in the routing table
 */
class KBucket {
public:
    /**
     * @brief Constructs a k-bucket
     * @param index The bucket index
     * @param maxSize The maximum number of nodes in the bucket
     */
    explicit KBucket(int index, size_t maxSize = DEFAULT_K_BUCKET_SIZE);

    /**
     * @brief Gets the bucket index
     * @return The bucket index
     */
    int getIndex() const;

    /**
     * @brief Gets the nodes in the bucket
     * @return The nodes in the bucket
     */
    const std::list<std::shared_ptr<Node>>& getNodes() const;

    /**
     * @brief Gets the number of nodes in the bucket
     * @return The number of nodes in the bucket
     */
    size_t size() const;

    /**
     * @brief Checks if the bucket is full
     * @return True if the bucket is full, false otherwise
     */
    bool isFull() const;

    /**
     * @brief Adds a node to the bucket
     * @param node The node to add
     * @return True if the node was added, false otherwise
     */
    bool addNode(std::shared_ptr<Node> node);

    /**
     * @brief Removes a node from the bucket
     * @param node The node to remove
     * @return True if the node was removed, false otherwise
     */
    bool removeNode(std::shared_ptr<Node> node);

    /**
     * @brief Finds a node in the bucket by ID
     * @param id The node ID to find
     * @return The node, or nullptr if not found
     */
    std::shared_ptr<Node> findNode(const NodeID& id) const;

    /**
     * @brief Gets the least recently seen node in the bucket
     * @return The least recently seen node, or nullptr if the bucket is empty
     */
    std::shared_ptr<Node> getLeastRecentlySeenNode();

    /**
     * @brief Gets the oldest node in the bucket
     * @return The oldest node, or nullptr if the bucket is empty
     */
    std::shared_ptr<Node> getOldestNode();

private:
    int m_index;
    size_t m_maxSize;
    std::list<std::shared_ptr<Node>> m_nodes;
};

/**
 * @brief Represents a routing table for the DHT network
 */
class RoutingTable {
public:
    /**
     * @brief Constructs a routing table
     * @param ownID The ID of the local node
     * @param kBucketSize The maximum number of nodes in each k-bucket
     */
    explicit RoutingTable(const NodeID& ownID, size_t kBucketSize = DEFAULT_K_BUCKET_SIZE);

    /**
     * @brief Gets the ID of the local node
     * @return The ID of the local node
     */
    const NodeID& getOwnID() const;

    /**
     * @brief Updates the own node ID
     * @param ownID The new own node ID
     */
    void updateOwnID(const NodeID& ownID);

    /**
     * @brief Adds a node to the routing table
     * @param node The node to add
     * @return True if the node was added, false otherwise
     */
    bool addNode(std::shared_ptr<Node> node);

    /**
     * @brief Removes a node from the routing table
     * @param id The ID of the node to remove
     * @return True if the node was removed, false otherwise
     */
    bool removeNode(const NodeID& id);

    /**
     * @brief Finds a node in the routing table by ID
     * @param id The node ID to find
     * @return The node, or nullptr if not found
     */
    std::shared_ptr<Node> findNode(const NodeID& id);

    /**
     * @brief Gets the bucket for a given node ID
     * @param id The node ID
     * @return The bucket
     */
    KBucket& getBucket(const NodeID& id);

    /**
     * @brief Gets the bucket at a given index
     * @param index The bucket index
     * @return The bucket
     */
    KBucket& getBucketByIndex(int index);

    /**
     * @brief Gets all buckets in the routing table
     * @return The buckets
     */
    const std::vector<KBucket>& getBuckets() const;

    /**
     * @brief Gets the closest nodes to a given ID
     * @param id The target ID
     * @param count The maximum number of nodes to return
     * @return The closest nodes
     */
    std::vector<std::shared_ptr<Node>> getClosestNodes(const NodeID& id, size_t count);

    /**
     * @brief Gets all nodes in the routing table
     * @return The nodes
     */
    std::vector<std::shared_ptr<Node>> getAllNodes();

    /**
     * @brief Gets the number of nodes in the routing table
     * @return The number of nodes
     */
    size_t size() const;

    /**
     * @brief Checks if the routing table is empty
     * @return True if the routing table is empty, false otherwise
     */
    bool isEmpty() const;

    /**
     * @brief Clears the routing table
     */
    void clear();

    /**
     * @brief Clears the routing table without locking the mutex
     * @note This method should only be called when the mutex is already locked
     */
    void clearNoLock();

    /**
     * @brief Gets the closest nodes to a target ID without locking the mutex
     * @param id The target ID
     * @param count The maximum number of nodes to return
     * @return The closest nodes to the target ID
     * @note This method should only be called when the mutex is already locked
     */
    std::vector<std::shared_ptr<Node>> getClosestNodesNoLock(const NodeID& id, size_t count) const;

    /**
     * @brief Adds a node to the routing table without locking the mutex
     * @param node The node to add
     * @return True if the node was added, false otherwise
     * @note This method should only be called when the mutex is already locked
     */
    bool addNodeNoLock(std::shared_ptr<Node> node);

    /**
     * @brief Finds a node in the routing table without locking the mutex
     * @param id The ID of the node to find
     * @return The node, or nullptr if not found
     * @note This method should only be called when the mutex is already locked
     */
    std::shared_ptr<Node> findNodeNoLock(const NodeID& id) const;

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
    std::vector<KBucket> m_buckets;
    mutable util::CheckedMutex m_mutex;
};

} // namespace dht_hunter::dht
