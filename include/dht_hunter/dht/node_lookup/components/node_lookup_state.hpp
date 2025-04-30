#pragma once

#include "dht_hunter/dht/types.hpp"
#include <functional>
#include <unordered_set>
#include <vector>
#include <string>

namespace dht_hunter::dht::node_lookup {

/**
 * @brief A lookup state for node lookups
 */
class NodeLookupState {
public:
    /**
     * @brief Constructs a node lookup state
     * @param targetID The target ID
     * @param callback The callback to call with the result
     */
    NodeLookupState(const NodeID& targetID, std::function<void(const std::vector<std::shared_ptr<Node>>&)> callback);

    /**
     * @brief Gets the target ID
     * @return The target ID
     */
    const NodeID& getTargetID() const;

    /**
     * @brief Gets the nodes
     * @return The nodes
     */
    const std::vector<std::shared_ptr<Node>>& getNodes() const;

    /**
     * @brief Gets the queried nodes
     * @return The queried nodes
     */
    const std::unordered_set<std::string>& getQueriedNodes() const;

    /**
     * @brief Gets the responded nodes
     * @return The responded nodes
     */
    const std::unordered_set<std::string>& getRespondedNodes() const;

    /**
     * @brief Gets the active queries
     * @return The active queries
     */
    const std::unordered_set<std::string>& getActiveQueries() const;

    /**
     * @brief Gets the iteration
     * @return The iteration
     */
    size_t getIteration() const;

    /**
     * @brief Gets the callback
     * @return The callback
     */
    const std::function<void(const std::vector<std::shared_ptr<Node>>&)>& getCallback() const;

    /**
     * @brief Sets the nodes
     * @param nodes The nodes
     */
    void setNodes(const std::vector<std::shared_ptr<Node>>& nodes);

    /**
     * @brief Adds a node to the nodes
     * @param node The node to add
     */
    void addNode(std::shared_ptr<Node> node);

    /**
     * @brief Adds a node to the queried nodes
     * @param nodeID The node ID to add
     */
    void addQueriedNode(const std::string& nodeID);

    /**
     * @brief Adds a node to the responded nodes
     * @param nodeID The node ID to add
     */
    void addRespondedNode(const std::string& nodeID);

    /**
     * @brief Adds a node to the active queries
     * @param nodeID The node ID to add
     */
    void addActiveQuery(const std::string& nodeID);

    /**
     * @brief Removes a node from the active queries
     * @param nodeID The node ID to remove
     */
    void removeActiveQuery(const std::string& nodeID);

    /**
     * @brief Increments the iteration
     */
    void incrementIteration();

    /**
     * @brief Checks if a node has been queried
     * @param nodeID The node ID to check
     * @return True if the node has been queried, false otherwise
     */
    bool hasBeenQueried(const std::string& nodeID) const;

    /**
     * @brief Checks if a node has responded
     * @param nodeID The node ID to check
     * @return True if the node has responded, false otherwise
     */
    bool hasResponded(const std::string& nodeID) const;

    /**
     * @brief Checks if a node has an active query
     * @param nodeID The node ID to check
     * @return True if the node has an active query, false otherwise
     */
    bool hasActiveQuery(const std::string& nodeID) const;

private:
    NodeID m_targetID;
    std::vector<std::shared_ptr<Node>> m_nodes;
    std::unordered_set<std::string> m_queriedNodes;
    std::unordered_set<std::string> m_respondedNodes;
    std::unordered_set<std::string> m_activeQueries;
    size_t m_iteration;
    std::function<void(const std::vector<std::shared_ptr<Node>>&)> m_callback;
};

} // namespace dht_hunter::dht::node_lookup
