#include "dht_hunter/dht/routing/dht_node_lookup.hpp"
#include "dht_hunter/dht/routing/dht_routing_manager.hpp"
#include "dht_hunter/dht/transactions/dht_transaction_manager.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>

DEFINE_COMPONENT_LOGGER("DHT", "NodeLookup")

namespace dht_hunter::dht {

//
// NodeLookup implementation
//

NodeLookup::NodeLookup(const std::string& id, const NodeID& targetID, NodeLookupCallback callback)
    : m_id(id),
      m_targetID(targetID),
      m_callback(callback),
      m_complete(false) {
}

const std::string& NodeLookup::getID() const {
    return m_id;
}

const NodeID& NodeLookup::getTargetID() const {
    return m_targetID;
}

std::vector<std::shared_ptr<Node>> NodeLookup::getClosestNodes() const {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    return m_closestNodes;
}

bool NodeLookup::addNode(std::shared_ptr<Node> node) {
    if (!node) {
        return false;
    }

    std::lock_guard<util::CheckedMutex> lock(m_mutex);

    // Check if the node is already in the list
    for (const auto& existingNode : m_closestNodes) {
        if (existingNode->getNodeID() == node->getNodeID()) {
            return false;
        }
    }

    // Add the node to the list
    m_closestNodes.push_back(node);

    // Sort the list by distance to the target ID
    std::sort(m_closestNodes.begin(), m_closestNodes.end(),
        [this](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
            return calculateDistance(a->getNodeID(), m_targetID) < calculateDistance(b->getNodeID(), m_targetID);
        });

    // Limit the list to the closest nodes
    if (m_closestNodes.size() > DEFAULT_LOOKUP_MAX_RESULTS) {
        m_closestNodes.resize(DEFAULT_LOOKUP_MAX_RESULTS);
    }

    return true;
}

void NodeLookup::markNodeAsQueried(const NodeID& nodeID) {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    m_queriedNodes.insert(nodeID);
}

void NodeLookup::markNodeAsResponded(const NodeID& nodeID) {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    m_respondedNodes.insert(nodeID);
}

void NodeLookup::markNodeAsFailed(const NodeID& nodeID) {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    m_failedNodes.insert(nodeID);
}

std::vector<std::shared_ptr<Node>> NodeLookup::getNextNodesToQuery(size_t count) {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);

    std::vector<std::shared_ptr<Node>> nodesToQuery;

    for (const auto& node : m_closestNodes) {
        // Skip nodes that have already been queried or failed
        if (m_queriedNodes.find(node->getNodeID()) != m_queriedNodes.end() ||
            m_failedNodes.find(node->getNodeID()) != m_failedNodes.end()) {
            continue;
        }

        nodesToQuery.push_back(node);

        if (nodesToQuery.size() >= count) {
            break;
        }
    }

    return nodesToQuery;
}

bool NodeLookup::isComplete() const {
    if (m_complete) {
        return true;
    }

    std::lock_guard<util::CheckedMutex> lock(m_mutex);

    // Check if we have queried all nodes
    for (const auto& node : m_closestNodes) {
        if (m_queriedNodes.find(node->getNodeID()) == m_queriedNodes.end() &&
            m_failedNodes.find(node->getNodeID()) == m_failedNodes.end()) {
            return false;
        }
    }

    // Check if we have received responses from all queried nodes
    for (const auto& nodeID : m_queriedNodes) {
        if (m_respondedNodes.find(nodeID) == m_respondedNodes.end() &&
            m_failedNodes.find(nodeID) == m_failedNodes.end()) {
            return false;
        }
    }

    return true;
}

void NodeLookup::complete() {
    // Only call the callback once
    if (m_complete.exchange(true)) {
        return;
    }

    // Call the callback with the closest nodes
    if (m_callback) {
        m_callback(getClosestNodes());
    }
}

//
// DHTNodeLookup implementation
//

DHTNodeLookup::DHTNodeLookup(const DHTNodeConfig& config,
                           const NodeID& nodeID,
                           std::shared_ptr<DHTRoutingManager> routingManager,
                           std::shared_ptr<DHTTransactionManager> transactionManager)
    : m_config(config),
      m_nodeID(nodeID),
      m_routingManager(routingManager),
      m_transactionManager(transactionManager) {
    getLogger()->info("Node lookup manager created");
}

std::string DHTNodeLookup::startLookup(const NodeID& targetID, NodeLookupCallback callback) {
    if (!m_routingManager || !m_transactionManager) {
        getLogger()->error("Cannot start lookup: Required components not available");
        return "";
    }

    // Generate a lookup ID
    std::string lookupID = generateLookupID();

    // Create a lookup
    auto lookup = std::make_shared<NodeLookup>(lookupID, targetID, callback);

    // Add the lookup to the map
    {
        std::lock_guard<util::CheckedMutex> lock(m_lookupsMutex);
        m_lookups[lookupID] = lookup;
    }

    // Get the closest nodes from the routing table
    auto closestNodes = m_routingManager->getClosestNodes(targetID, DEFAULT_LOOKUP_MAX_RESULTS);

    // Add the nodes to the lookup
    for (const auto& node : closestNodes) {
        lookup->addNode(node);
    }

    // Continue the lookup
    continueLookup(lookup);

    return lookupID;
}

bool DHTNodeLookup::handleFindNodeResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender) {
    if (!response) {
        getLogger()->error("Invalid response");
        return false;
    }

    // Find the lookup
    auto lookup = findLookupByTransactionID(response->getTransactionID());
    if (!lookup) {
        getLogger()->warning("Cannot handle find_node response: Lookup not found for transaction {}",
                      response->getTransactionID());
        return false;
    }

    // Get the sender's node ID
    if (!response->getNodeID().has_value()) {
        getLogger()->warning("Find_node response from {} has no node ID", sender.toString());
        return false;
    }

    // Mark the node as responded
    lookup->markNodeAsResponded(response->getNodeID().value());

    // Get the nodes from the response
    auto nodes = response->getNodes();
    if (nodes.empty()) {
        getLogger()->debug("Find_node response from {} has no nodes", sender.toString());
    } else {
        getLogger()->debug("Find_node response from {} has {} nodes", sender.toString(), nodes.size());
    }

    // Add the nodes to the lookup
    for (const auto& node : nodes) {
        lookup->addNode(node);
    }

    // Continue the lookup
    continueLookup(lookup);

    return true;
}

bool DHTNodeLookup::handleFindNodeError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender) {
    if (!error) {
        getLogger()->error("Invalid error");
        return false;
    }

    // Find the lookup
    auto lookup = findLookupByTransactionID(error->getTransactionID());
    if (!lookup) {
        getLogger()->warning("Cannot handle find_node error: Lookup not found for transaction {}",
                      error->getTransactionID());
        return false;
    }

    // Get the sender's node ID
    if (!error->getNodeID().has_value()) {
        getLogger()->warning("Find_node error from {} has no node ID", sender.toString());
        return false;
    }

    // Mark the node as failed
    lookup->markNodeAsFailed(error->getNodeID().value());

    // Continue the lookup
    continueLookup(lookup);

    return true;
}

bool DHTNodeLookup::handleFindNodeTimeout(const std::string& transactionID) {
    // Find the lookup
    auto lookup = findLookupByTransactionID(transactionID);
    if (!lookup) {
        getLogger()->warning("Cannot handle find_node timeout: Lookup not found for transaction {}", transactionID);
        return false;
    }

    // Remove the transaction from the map
    {
        std::lock_guard<util::CheckedMutex> lock(m_lookupsMutex);
        m_transactionToLookup.erase(transactionID);
    }

    // Continue the lookup
    continueLookup(lookup);

    return true;
}

void DHTNodeLookup::continueLookup(std::shared_ptr<NodeLookup> lookup) {
    if (!lookup) {
        getLogger()->error("Cannot continue lookup: Invalid lookup");
        return;
    }

    // Check if the lookup is complete
    if (lookup->isComplete()) {
        getLogger()->debug("Lookup {} complete", lookup->getID());
        lookup->complete();

        // Remove the lookup from the map
        {
            std::lock_guard<util::CheckedMutex> lock(m_lookupsMutex);
            m_lookups.erase(lookup->getID());
        }

        return;
    }

    // Get the next nodes to query
    auto nodesToQuery = lookup->getNextNodesToQuery(DEFAULT_LOOKUP_ALPHA);
    if (nodesToQuery.empty()) {
        getLogger()->debug("No more nodes to query for lookup {}", lookup->getID());
        lookup->complete();

        // Remove the lookup from the map
        {
            std::lock_guard<util::CheckedMutex> lock(m_lookupsMutex);
            m_lookups.erase(lookup->getID());
        }

        return;
    }

    // Query the nodes
    for (const auto& node : nodesToQuery) {
        // Mark the node as queried
        lookup->markNodeAsQueried(node->getNodeID());

        // Create a find_node query
        auto query = std::make_shared<FindNodeQuery>(lookup->getTargetID());
        query->setNodeID(m_nodeID);

        // Create a transaction
        std::string transactionID = m_transactionManager->createTransaction(
            query,
            node->getEndpoint(),
            [this, node](std::shared_ptr<ResponseMessage> response) {
                // Handle the response
                handleFindNodeResponse(response, node->getEndpoint());
            },
            [this, node](std::shared_ptr<ErrorMessage> error) {
                // Handle the error
                handleFindNodeError(error, node->getEndpoint());
            },
            [this, transactionID = std::string()]() {
                // Handle the timeout
                if (!transactionID.empty()) {
                    handleFindNodeTimeout(transactionID);
                }
            }
        );

        if (transactionID.empty()) {
            getLogger()->error("Failed to create transaction for find_node to {}", node->getEndpoint().toString());
            continue;
        }

        // Add the transaction to the map
        {
            std::lock_guard<util::CheckedMutex> lock(m_lookupsMutex);
            m_transactionToLookup[transactionID] = lookup->getID();
        }

        // Send the query
        if (!m_transactionManager->findTransaction(transactionID)) {
            getLogger()->error("Failed to find transaction {} for find_node to {}",
                         transactionID, node->getEndpoint().toString());
            continue;
        }
    }
}

std::string DHTNodeLookup::generateLookupID() const {
    // Generate a random lookup ID
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<uint32_t> dist;
    
    // Generate a random 32-bit integer
    uint32_t random = dist(rng);
    
    // Convert to a hex string
    std::stringstream ss;
    ss << "lookup_" << std::hex << std::setw(8) << std::setfill('0') << random;
    
    return ss.str();
}

std::shared_ptr<NodeLookup> DHTNodeLookup::findLookupByTransactionID(const std::string& transactionID) {
    std::lock_guard<util::CheckedMutex> lock(m_lookupsMutex);
    
    auto it = m_transactionToLookup.find(transactionID);
    if (it == m_transactionToLookup.end()) {
        return nullptr;
    }
    
    auto lookupIt = m_lookups.find(it->second);
    if (lookupIt == m_lookups.end()) {
        return nullptr;
    }
    
    return lookupIt->second;
}

} // namespace dht_hunter::dht
