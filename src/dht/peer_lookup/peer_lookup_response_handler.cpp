#include "dht_hunter/dht/peer_lookup/peer_lookup_response_handler.hpp"
#include "dht_hunter/dht/peer_lookup/peer_lookup.hpp"
#include "dht_hunter/dht/storage/peer_storage.hpp"
#include <algorithm>

namespace dht_hunter::dht {

PeerLookupResponseHandler::PeerLookupResponseHandler(const DHTConfig& config,
                                                   const NodeID& nodeID,
                                                   std::shared_ptr<RoutingTable> routingTable,
                                                   std::shared_ptr<PeerStorage> peerStorage)
    : m_config(config),
      m_nodeID(nodeID),
      m_routingTable(routingTable),
      m_peerStorage(peerStorage) {
}

bool PeerLookupResponseHandler::handleResponse(
    const std::string& /*lookupID*/,
    PeerLookupState& lookup,
    std::shared_ptr<GetPeersResponse> response,
    const EndPoint& /*sender*/) {

    // Variables to store data we'll need after processing
    std::vector<std::shared_ptr<Node>> nodesToAdd;
    std::vector<EndPoint> peersToAdd;
    bool shouldSendQueries = false;

    // Get the node ID from the response
    if (!response->getNodeID()) {
        return false;
    }

    std::string nodeIDStr = nodeIDToString(response->getNodeID().value());

    // Remove the node from the active queries
    lookup.activeQueries.erase(nodeIDStr);

    // Add the node to the responded nodes
    lookup.respondedNodes.insert(nodeIDStr);

    // Store the token
    lookup.nodeTokens[nodeIDStr] = response->getToken();

    // Process the peers from the response
    if (response->hasPeers()) {
        for (const auto& peer : response->getPeers()) {
            // Check if the peer is already in the list
            auto peerIt = std::find(lookup.peers.begin(), lookup.peers.end(), peer);

            if (peerIt == lookup.peers.end()) {
                // Add the peer to the lookup's peer list
                lookup.peers.push_back(peer);

                // Store the peer for adding to the peer storage
                peersToAdd.push_back(peer);
            }
        }
    }

    // Process the nodes from the response
    if (response->hasNodes()) {
        for (const auto& node : response->getNodes()) {
            // Skip our own node
            if (node->getID() == m_nodeID) {
                continue;
            }

            // Check if the node is already in the list
            auto nodeIt = std::find_if(lookup.nodes.begin(), lookup.nodes.end(),
                [&node](const std::shared_ptr<Node>& existingNode) {
                    return existingNode->getID() == node->getID();
                });

            if (nodeIt == lookup.nodes.end()) {
                // Add the node to the lookup's node list
                lookup.nodes.push_back(node);

                // Store the node for adding to the routing table
                nodesToAdd.push_back(node);
            }
        }
    }

    // Set flag to send more queries
    shouldSendQueries = true;

    // Add the peers to the peer storage
    if (m_peerStorage && !peersToAdd.empty()) {
        for (const auto& peer : peersToAdd) {
            m_peerStorage->addPeer(lookup.infoHash, peer);
        }
    }

    // Add the nodes to the routing table
    if (m_routingTable && !nodesToAdd.empty()) {
        for (const auto& node : nodesToAdd) {
            m_routingTable->addNode(node);
        }
    }

    return shouldSendQueries;
}

bool PeerLookupResponseHandler::handleError(
    PeerLookupState& lookup,
    std::shared_ptr<ErrorMessage> /*error*/,
    const EndPoint& sender) {

    // Find the node
    auto nodeIt = std::find_if(lookup.nodes.begin(), lookup.nodes.end(),
        [&sender](const std::shared_ptr<Node>& node) {
            return node->getEndpoint() == sender;
        });

    if (nodeIt == lookup.nodes.end()) {
        return false;
    }

    std::string nodeIDStr = nodeIDToString((*nodeIt)->getID());

    // Remove the node from the active queries
    lookup.activeQueries.erase(nodeIDStr);

    // Send more queries
    return true;
}

bool PeerLookupResponseHandler::handleTimeout(
    PeerLookupState& lookup,
    const NodeID& nodeID) {

    std::string nodeIDStr = nodeIDToString(nodeID);

    // Remove the node from the active queries
    lookup.activeQueries.erase(nodeIDStr);

    // Send more queries
    return true;
}

bool PeerLookupResponseHandler::handleAnnounceResponse(
    PeerLookupState& lookup,
    std::shared_ptr<AnnouncePeerResponse> response,
    const EndPoint& /*sender*/) {

    // Get the node ID from the response
    if (!response->getNodeID()) {
        return false;
    }

    std::string nodeIDStr = nodeIDToString(response->getNodeID().value());

    // Remove the node from the active announcements
    lookup.activeAnnouncements.erase(nodeIDStr);

    // Add the node to the announced nodes
    lookup.announcedNodes.insert(nodeIDStr);

    // Check if the announcement is complete
    return lookup.activeAnnouncements.empty();
}

bool PeerLookupResponseHandler::handleAnnounceError(
    PeerLookupState& lookup,
    std::shared_ptr<ErrorMessage> /*error*/,
    const EndPoint& sender) {

    // Find the node
    auto nodeIt = std::find_if(lookup.nodes.begin(), lookup.nodes.end(),
        [&sender](const std::shared_ptr<Node>& node) {
            return node->getEndpoint() == sender;
        });

    if (nodeIt == lookup.nodes.end()) {
        return false;
    }

    std::string nodeIDStr = nodeIDToString((*nodeIt)->getID());

    // Remove the node from the active announcements
    lookup.activeAnnouncements.erase(nodeIDStr);

    // Check if the announcement is complete
    return lookup.activeAnnouncements.empty();
}

bool PeerLookupResponseHandler::handleAnnounceTimeout(
    PeerLookupState& lookup,
    const NodeID& nodeID) {

    std::string nodeIDStr = nodeIDToString(nodeID);

    // Remove the node from the active announcements
    lookup.activeAnnouncements.erase(nodeIDStr);

    // Check if the announcement is complete
    return lookup.activeAnnouncements.empty();
}

void PeerLookupResponseHandler::completeLookup(PeerLookupState& lookup) const {
    // Call the callback
    if (lookup.lookupCallback) {
        lookup.lookupCallback(lookup.peers);
    }
}

} // namespace dht_hunter::dht
