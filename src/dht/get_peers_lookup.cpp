#include "dht_hunter/dht/dht_node.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include "dht_hunter/crawler/infohash_collector.hpp"
#include <algorithm>
#include <sstream>

DEFINE_COMPONENT_LOGGER("DHT", "GetPeersLookup")

namespace dht_hunter::dht {
bool DHTNode::startGetPeersLookup(const InfoHash& infoHash, GetPeersLookupCallback callback) {
    // Create a new lookup
    auto lookup = std::make_shared<GetPeersLookup>();
    lookup->infoHash = infoHash;
    lookup->activeQueries = 0;
    lookup->iterations = 0;
    lookup->callback = callback;
    lookup->completed = false;
    // Generate a unique ID for the lookup
    std::string lookupID = nodeIDToString(infoHash);
    // Add the lookup to the map
    {
        std::lock_guard<util::CheckedMutex> lock(m_getPeersLookupsLock);
        m_getPeersLookups[lookupID] = lookup;
    }
    // Start with nodes from our routing table
    auto closestNodes = m_routingTable.getClosestNodes(infoHash, LOOKUP_MAX_RESULTS);
    // If we don't have any nodes in our routing table, we can't do a lookup
    if (closestNodes.empty()) {
        getLogger()->error("Cannot start get_peers lookup: No nodes in routing table");
        // Remove the lookup from the map
        {
            std::lock_guard<util::CheckedMutex> lock(m_getPeersLookupsLock);
            m_getPeersLookups.erase(lookupID);
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
            lookupNode.distance = calculateDistance(infoHash, node->getID());
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
    continueGetPeersLookup(lookup);
    return true;
}
void DHTNode::continueGetPeersLookup(std::shared_ptr<GetPeersLookup> lookup) {
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
            getLogger()->debug("Get_peers lookup reached maximum iterations");
            lookup->completed = true;
            completeGetPeersLookup(lookup);
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
            getLogger()->debug("Get_peers lookup completed: no more nodes to query");
            lookup->completed = true;
            completeGetPeersLookup(lookup);
        }
        return;
    }
    // Query the nodes
    for (auto* node : nodesToQuery) {
        // Send a get_peers query to the node
        getPeers(lookup->infoHash, node->node->getEndpoint(),
            [this, lookup, node](std::shared_ptr<ResponseMessage> response) {
                // Cast to GetPeersResponse
                auto getPeersResponse = std::dynamic_pointer_cast<GetPeersResponse>(response);
                if (getPeersResponse) {
                    // Handle the response
                    handleGetPeersLookupResponse(lookup, getPeersResponse, node->node->getEndpoint());
                }
                // Decrement the active queries count
                {
                    std::lock_guard<util::CheckedMutex> lock(lookup->mutex);
                    lookup->activeQueries--;
                    // Mark the node as responded
                    node->responded = true;
                }
                // Continue the lookup
                continueGetPeersLookup(lookup);
            },
            [this, lookup, node](std::shared_ptr<ErrorMessage> error) {
                getLogger()->debug("Get_peers lookup received error from {}: {} ({})",
                             node->node->getEndpoint().toString(),
                             error->getMessage(), error->getCode());
                // Decrement the active queries count
                {
                    std::lock_guard<util::CheckedMutex> lock(lookup->mutex);
                    lookup->activeQueries--;
                }
                // Continue the lookup
                continueGetPeersLookup(lookup);
            },
            [this, lookup, node]() {
                getLogger()->debug("Get_peers lookup timed out for {}",
                             node->node->getEndpoint().toString());
                // Decrement the active queries count
                {
                    std::lock_guard<util::CheckedMutex> lock(lookup->mutex);
                    lookup->activeQueries--;
                }
                // Continue the lookup
                continueGetPeersLookup(lookup);
            });
    }
}
void DHTNode::handleGetPeersLookupResponse(std::shared_ptr<GetPeersLookup> lookup,
                                         std::shared_ptr<GetPeersResponse> response,
                                         const network::EndPoint& endpoint) {
    // Forward the infohash to the collector if set
    if (m_infoHashCollector) {
        m_infoHashCollector->addInfoHash(lookup->infoHash);
    }
    std::lock_guard<util::CheckedMutex> lock(lookup->mutex);
    // Check if we got peers
    const auto& peers = response->getPeers();
    if (!peers.empty()) {
        getLogger()->debug("Got {} peers from {}", peers.size(), endpoint.toString());
        // Add the peers to the lookup
        for (const auto& peer : peers) {
            // Check if we already have this peer
            auto it = std::find(lookup->peers.begin(), lookup->peers.end(), peer);
            if (it == lookup->peers.end()) {
                lookup->peers.push_back(peer);
            }
            // Store the peer locally
            storePeer(lookup->infoHash, peer);
        }
    }
    // Get the token (we'll need it for announce_peer)
    const auto& token = response->getToken();
    if (!token.empty()) {
        // Store the token from the closest node
        if (lookup->token.empty()) {
            lookup->token = token;
        }
    }
    // Get the nodes from the response
    const auto& nodes = response->getNodes();
    if (!nodes.empty()) {
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
                lookupNode.distance = calculateDistance(lookup->infoHash, node.id);
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
    // If we have enough peers, we can complete the lookup
    if (lookup->peers.size() >= LOOKUP_MAX_RESULTS) {
        getLogger()->debug("Get_peers lookup found enough peers: {}", lookup->peers.size());
        lookup->completed = true;
        completeGetPeersLookup(lookup);
    }
}
void DHTNode::completeGetPeersLookup(std::shared_ptr<GetPeersLookup> lookup) {
    // Get the peers and closest nodes
    std::vector<network::EndPoint> peers;
    std::vector<std::shared_ptr<Node>> closestNodes;
    std::string token;
    {
        std::lock_guard<util::CheckedMutex> lock(lookup->mutex);
        // Copy the peers
        peers = lookup->peers;
        // Get the token
        token = lookup->token;
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
                if (!node.responded && std::find(closestNodes.begin(), closestNodes.end(), node.node) == closestNodes.end()) {
                    closestNodes.push_back(node.node);
                    if (closestNodes.size() >= LOOKUP_MAX_RESULTS) {
                        break;
                    }
                }
            }
        }
    }
    // Call the callback
    lookup->callback(peers, closestNodes, token);
    // Remove the lookup from the map
    {
        std::lock_guard<util::CheckedMutex> lock(m_getPeersLookupsLock);
        m_getPeersLookups.erase(nodeIDToString(lookup->infoHash));
    }
}
} // namespace dht_hunter::dht
