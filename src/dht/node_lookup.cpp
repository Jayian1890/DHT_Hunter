#include "dht_hunter/dht/dht_node.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include "dht_hunter/util/mutex_utils.hpp"
#include <algorithm>
#include <sstream>

DEFINE_COMPONENT_LOGGER("DHT", "NodeLookup")

namespace dht_hunter::dht {
bool DHTNode::findClosestNodes(const NodeID& targetID, NodeLookupCallback callback) {
    if (!m_running) {
        getLogger()->error("Cannot find closest nodes: DHT node not running");
        return false;
    }
    if (!callback) {
        getLogger()->error("Cannot find closest nodes: No callback provided");
        return false;
    }
    return startNodeLookup(targetID, callback);
}
bool DHTNode::findPeers(const InfoHash& infoHash, GetPeersLookupCallback callback) {
    if (!m_running) {
        getLogger()->error("Cannot find peers: DHT node not running");
        return false;
    }
    if (!callback) {
        getLogger()->error("Cannot find peers: No callback provided");
        return false;
    }
    return startGetPeersLookup(infoHash, callback);
}
bool DHTNode::announceAsPeer(const InfoHash& infoHash, uint16_t port, std::function<void(bool)> callback) {
    if (!m_running) {
        getLogger()->error("Cannot announce as peer: DHT node not running");
        return false;
    }
    if (!callback) {
        getLogger()->error("Cannot announce as peer: No callback provided");
        return false;
    }
    // First, find peers to get tokens from the closest nodes
    return findPeers(infoHash, [this, infoHash, port, callback](
        const std::vector<network::EndPoint>& peers,
        const std::vector<std::shared_ptr<Node>>& nodes,
        const std::string& token) {
        (void)peers; // Unused parameter
        // If we didn't find any nodes, we can't announce
        if (nodes.empty()) {
            getLogger()->error("Cannot announce as peer: No nodes found");
            callback(false);
            return;
        }
        // We need to announce to the closest nodes
        size_t successCount = 0;
        size_t totalCount = std::min(nodes.size(), LOOKUP_MAX_RESULTS);
        std::atomic<size_t> completedCount(0);
        for (size_t i = 0; i < totalCount; ++i) {
            const auto& node = nodes[i];
            // Get the token for this node (we should have received one during the get_peers lookup)
            // For simplicity, we'll use the same token for all nodes, which might not work for all nodes
            announcePeer(infoHash, port, token, node->getEndpoint(),
                [&successCount, &completedCount, totalCount, callback](std::shared_ptr<ResponseMessage> response) {
                    (void)response; // Unused parameter
                    // Successful announcement
                    successCount++;
                    completedCount++;
                    // If we've completed all announcements, call the callback
                    if (completedCount == totalCount) {
                        callback(successCount > 0);
                    }
                },
                [&successCount, &completedCount, totalCount, callback](std::shared_ptr<ErrorMessage> error) {
                    (void)error; // Unused parameter
                    // Failed announcement
                    completedCount++;
                    // If we've completed all announcements, call the callback
                    if (completedCount == totalCount) {
                        callback(successCount > 0);
                    }
                },
                [&successCount, &completedCount, totalCount, callback]() {
                    // Timed out announcement
                    completedCount++;
                    // If we've completed all announcements, call the callback
                    if (completedCount == totalCount) {
                        callback(successCount > 0);
                    }
                });
        }
    });
}
bool DHTNode::startNodeLookup(const NodeID& targetID, NodeLookupCallback callback) {
    // Create a new lookup
    auto lookup = std::make_shared<NodeLookup>();
    lookup->targetID = targetID;
    lookup->activeQueries = 0;
    lookup->iterations = 0;
    lookup->callback = callback;
    lookup->completed = false;
    // Generate a unique ID for the lookup
    std::string lookupID = nodeIDToString(targetID);
    // Add the lookup to the map
    {
        std::lock_guard<util::CheckedMutex> lock(m_nodeLookupsLock);
        m_nodeLookups[lookupID] = lookup;
    }
    // Start with nodes from our routing table
    auto closestNodes = m_routingTable.getClosestNodes(targetID, LOOKUP_MAX_RESULTS);
    // If we don't have any nodes in our routing table, we can't do a lookup
    if (closestNodes.empty()) {
        getLogger()->error("Cannot start node lookup: No nodes in routing table");
        // Remove the lookup from the map
        {
            std::lock_guard<util::CheckedMutex> lock(m_nodeLookupsLock);
            m_nodeLookups.erase(lookupID);
        }
        return false;
    }
    // Add the nodes to the lookup
    {
        std::lock_guard<util::CheckedMutex> lock(lookup->mutex);
        for (const auto& node : closestNodes) {
            LookupNode lookupNode;
            lookupNode.node = node;
            lookupNode.queried = false;
            lookupNode.responded = false;
            lookupNode.distance = calculateDistance(targetID, node->getID());
            lookup->nodes.push_back(lookupNode);
        }
        // Sort nodes by distance to the target
        std::sort(lookup->nodes.begin(), lookup->nodes.end(),
            [](const LookupNode& a, const LookupNode& b) {
                return std::lexicographical_compare(
                    a.distance.begin(), a.distance.end(),
                    b.distance.begin(), b.distance.end()
                );
            });
    }
    // Continue the lookup
    continueNodeLookup(lookup);
    return true;
}
void DHTNode::continueNodeLookup(std::shared_ptr<NodeLookup> lookup) {
    // Check if the lookup has completed
    {
        std::lock_guard<util::CheckedMutex> lock(lookup->mutex);
        if (lookup->completed) {
            return;
        }
        // Increment the iteration count
        lookup->iterations++;
        // If we've reached the maximum number of iterations, complete the lookup
        if (lookup->iterations >= LOOKUP_MAX_ITERATIONS) {
            getLogger()->debug("Node lookup reached maximum iterations");
            lookup->completed = true;
            completeNodeLookup(lookup);
            return;
        }
    }
    // Find the closest nodes that haven't been queried yet
    std::vector<LookupNode*> nodesToQuery;
    {
        std::lock_guard<util::CheckedMutex> lock(lookup->mutex);
        for (auto& node : lookup->nodes) {
            if (!node.queried) {
                nodesToQuery.push_back(&node);
                // Mark the node as queried
                node.queried = true;
                // Increment the active queries count
                lookup->activeQueries++;
                // Only query alpha nodes at a time
                if (nodesToQuery.size() >= LOOKUP_ALPHA) {
                    break;
                }
            }
        }
    }
    // If there are no nodes to query, check if we're done
    if (nodesToQuery.empty()) {
        std::lock_guard<util::CheckedMutex> lock(lookup->mutex);
        // If there are no active queries, we're done
        if (lookup->activeQueries == 0) {
            getLogger()->debug("Node lookup completed: no more nodes to query");
            lookup->completed = true;
            completeNodeLookup(lookup);
        }
        return;
    }
    // Query the nodes
    for (auto* node : nodesToQuery) {
        // Make a copy of the node's endpoint to avoid use-after-free
        network::EndPoint nodeEndpoint = node->node->getEndpoint();
        // Store the node ID for later lookup
        NodeID nodeID = node->node->getID();

        // Send a find_node query to the node
        findNode(lookup->targetID, nodeEndpoint,
            [this, lookup, nodeEndpoint, nodeID](std::shared_ptr<ResponseMessage> response) {
                // Cast to FindNodeResponse
                auto findNodeResponse = std::dynamic_pointer_cast<FindNodeResponse>(response);
                if (findNodeResponse) {
                    // Handle the response
                    handleNodeLookupResponse(lookup, findNodeResponse, nodeEndpoint);
                }
                // Decrement the active queries count
                {
                    std::lock_guard<std::mutex> lock(lookup->mutex);
                    lookup->activeQueries--;

                    // Find the node by ID and mark it as responded
                    for (auto& lookupNode : lookup->nodes) {
                        if (lookupNode.node->getID() == nodeID) {
                            lookupNode.responded = true;
                            break;
                        }
                    }
                }
                // Continue the lookup
                continueNodeLookup(lookup);
            },
            [this, lookup, nodeEndpoint, nodeID](std::shared_ptr<ErrorMessage> error) {
                getLogger()->debug("Node lookup received error from {}: {} ({})",
                             nodeEndpoint.toString(),
                             error->getMessage(), error->getCode());
                // Decrement the active queries count
                {
                    std::lock_guard<util::CheckedMutex> lock(lookup->mutex);
                    lookup->activeQueries--;

                    // Find the node by ID and mark it as responded
                    for (auto& lookupNode : lookup->nodes) {
                        if (lookupNode.node->getID() == nodeID) {
                            lookupNode.responded = true;
                            break;
                        }
                    }
                }
                // Continue the lookup
                continueNodeLookup(lookup);
            },
            [this, lookup, nodeEndpoint, nodeID]() {
                getLogger()->debug("Node lookup timed out for {}",
                             nodeEndpoint.toString());
                // Decrement the active queries count
                {
                    std::lock_guard<util::CheckedMutex> lock(lookup->mutex);
                    lookup->activeQueries--;

                    // Find the node by ID and mark it as responded
                    for (auto& lookupNode : lookup->nodes) {
                        if (lookupNode.node->getID() == nodeID) {
                            lookupNode.responded = true;
                            break;
                        }
                    }
                }
                // Continue the lookup
                continueNodeLookup(lookup);
            });
    }
}
void DHTNode::handleNodeLookupResponse(std::shared_ptr<NodeLookup> lookup,
                                     std::shared_ptr<FindNodeResponse> response,
                                     const network::EndPoint& endpoint) {
    (void)endpoint; // Unused parameter
    // Get the nodes from the response
    const auto& nodes = response->getNodes();
    // If there are no nodes, just continue the lookup
    if (nodes.empty()) {
        return;
    }
    // Add the nodes to the lookup
    {
        std::lock_guard<util::CheckedMutex> lock(lookup->mutex);
        for (const auto& node : nodes) {
            // Skip nodes with our own ID
            if (node.id == m_nodeID) {
                continue;
            }
            // Check if we already have this node
            auto it = std::find_if(lookup->nodes.begin(), lookup->nodes.end(),
                [&node](const LookupNode& lookupNode) {
                    return lookupNode.node->getID() == node.id;
                });
            if (it == lookup->nodes.end()) {
                // Add the node to our routing table
                addNode(node.id, node.endpoint);
                // Create a new node for the lookup
                auto newNode = std::make_shared<Node>(node.id, node.endpoint);
                // Add the node to the lookup
                LookupNode lookupNode;
                lookupNode.node = newNode;
                lookupNode.queried = false;
                lookupNode.responded = false;
                lookupNode.distance = calculateDistance(lookup->targetID, node.id);
                lookup->nodes.push_back(lookupNode);
            }
        }
        // Sort nodes by distance to the target
        std::sort(lookup->nodes.begin(), lookup->nodes.end(),
            [](const LookupNode& a, const LookupNode& b) {
                return std::lexicographical_compare(
                    a.distance.begin(), a.distance.end(),
                    b.distance.begin(), b.distance.end()
                );
            });
        // Limit the number of nodes
        if (lookup->nodes.size() > LOOKUP_MAX_RESULTS * 3) {
            lookup->nodes.resize(LOOKUP_MAX_RESULTS * 3);
        }
    }
}
void DHTNode::completeNodeLookup(std::shared_ptr<NodeLookup> lookup) {
    // Get the closest nodes
    std::vector<std::shared_ptr<Node>> closestNodes;
    {
        std::lock_guard<util::CheckedMutex> lock(lookup->mutex);
        // Sort nodes by distance to the target
        std::sort(lookup->nodes.begin(), lookup->nodes.end(),
            [](const LookupNode& a, const LookupNode& b) {
                return std::lexicographical_compare(
                    a.distance.begin(), a.distance.end(),
                    b.distance.begin(), b.distance.end()
                );
            });
        // Get the closest nodes that responded
        for (const auto& node : lookup->nodes) {
            if (node.responded) {
                closestNodes.push_back(node.node);
                if (closestNodes.size() >= LOOKUP_MAX_RESULTS) {
                    break;
                }
            }
        }
        // If we don't have enough nodes that responded, add nodes that didn't respond
        if (closestNodes.size() < LOOKUP_MAX_RESULTS) {
            for (const auto& node : lookup->nodes) {
                if (!node.responded) {
                    closestNodes.push_back(node.node);
                    if (closestNodes.size() >= LOOKUP_MAX_RESULTS) {
                        break;
                    }
                }
            }
        }
    }
    // Call the callback
    lookup->callback(closestNodes);
    // Remove the lookup from the map
    {
        std::lock_guard<util::CheckedMutex> lock(m_nodeLookupsLock);
        m_nodeLookups.erase(nodeIDToString(lookup->targetID));
    }
}
} // namespace dht_hunter::dht
