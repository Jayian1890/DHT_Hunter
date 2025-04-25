#include "dht_hunter/dht/routing/dht_peer_lookup.hpp"
#include "dht_hunter/dht/routing/dht_routing_manager.hpp"
#include "dht_hunter/dht/transactions/dht_transaction_manager.hpp"
#include "dht_hunter/dht/storage/dht_peer_storage.hpp"
#include "dht_hunter/dht/storage/dht_token_manager.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>

DEFINE_COMPONENT_LOGGER("DHT", "PeerLookup")

namespace dht_hunter::dht {

//
// PeerLookup implementation
//

PeerLookup::PeerLookup(const std::string& id, const InfoHash& infoHash, GetPeersLookupCallback callback)
    : m_id(id),
      m_infoHash(infoHash),
      m_callback(callback),
      m_complete(false) {
}

const std::string& PeerLookup::getID() const {
    return m_id;
}

const InfoHash& PeerLookup::getInfoHash() const {
    return m_infoHash;
}

std::vector<std::shared_ptr<Node>> PeerLookup::getClosestNodes() const {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    return m_closestNodes;
}

std::vector<network::EndPoint> PeerLookup::getPeers() const {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    return m_peers;
}

std::unordered_map<NodeID, std::string> PeerLookup::getTokens() const {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    return m_tokens;
}

bool PeerLookup::addNode(std::shared_ptr<Node> node) {
    if (!node) {
        return false;
    }

    std::lock_guard<util::CheckedMutex> lock(m_mutex);

    // Check if the node is already in the list
    for (const auto& existingNode : m_closestNodes) {
        if (existingNode->getID() == node->getID()) {
            return false;
        }
    }

    // Add the node to the list
    m_closestNodes.push_back(node);

    // Sort the list by distance to the info hash
    std::sort(m_closestNodes.begin(), m_closestNodes.end(),
        [this](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
            return calculateDistance(a->getID(), m_infoHash) < calculateDistance(b->getID(), m_infoHash);
        });

    // Limit the list to the closest nodes
    if (m_closestNodes.size() > DEFAULT_LOOKUP_MAX_RESULTS) {
        m_closestNodes.resize(DEFAULT_LOOKUP_MAX_RESULTS);
    }

    return true;
}

bool PeerLookup::addPeer(const network::EndPoint& peer) {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);

    // Check if the peer is already in the list
    for (const auto& existingPeer : m_peers) {
        if (existingPeer == peer) {
            return false;
        }
    }

    // Add the peer to the list
    m_peers.push_back(peer);
    return true;
}

void PeerLookup::addToken(const NodeID& nodeID, const std::string& token) {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    m_tokens[nodeID] = token;
}

void PeerLookup::markNodeAsQueried(const NodeID& nodeID) {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    m_queriedNodes.insert(nodeID);
}

void PeerLookup::markNodeAsResponded(const NodeID& nodeID) {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    m_respondedNodes.insert(nodeID);
}

void PeerLookup::markNodeAsFailed(const NodeID& nodeID) {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);
    m_failedNodes.insert(nodeID);
}

std::vector<std::shared_ptr<Node>> PeerLookup::getNextNodesToQuery(size_t count) {
    std::lock_guard<util::CheckedMutex> lock(m_mutex);

    std::vector<std::shared_ptr<Node>> nodesToQuery;

    for (const auto& node : m_closestNodes) {
        // Skip nodes that have already been queried or failed
        if (m_queriedNodes.find(node->getID()) != m_queriedNodes.end() ||
            m_failedNodes.find(node->getID()) != m_failedNodes.end()) {
            continue;
        }

        nodesToQuery.push_back(node);

        if (nodesToQuery.size() >= count) {
            break;
        }
    }

    return nodesToQuery;
}

bool PeerLookup::isComplete() const {
    if (m_complete) {
        return true;
    }

    std::lock_guard<util::CheckedMutex> lock(m_mutex);

    // If we found peers, the lookup is complete
    if (!m_peers.empty()) {
        return true;
    }

    // Check if we have queried all nodes
    for (const auto& node : m_closestNodes) {
        if (m_queriedNodes.find(node->getID()) == m_queriedNodes.end() &&
            m_failedNodes.find(node->getID()) == m_failedNodes.end()) {
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

void PeerLookup::complete() {
    // Only call the callback once
    if (m_complete.exchange(true)) {
        return;
    }

    // Call the callback with the peers and closest nodes
    if (m_callback) {
        auto peers = getPeers();
        auto closestNodes = getClosestNodes();

        // Get the token for the closest node
        std::string token;
        if (!closestNodes.empty()) {
            auto tokens = getTokens();
            auto it = tokens.find(closestNodes[0]->getID());
            if (it != tokens.end()) {
                token = it->second;
            }
        }

        m_callback(peers, closestNodes, token);
    }
}

//
// DHTPeerLookup implementation
//

DHTPeerLookup::DHTPeerLookup(const DHTNodeConfig& config,
                           const NodeID& nodeID,
                           std::shared_ptr<DHTRoutingManager> routingManager,
                           std::shared_ptr<DHTTransactionManager> transactionManager,
                           std::shared_ptr<DHTPeerStorage> peerStorage,
                           std::shared_ptr<DHTTokenManager> tokenManager)
    : m_config(config),
      m_nodeID(nodeID),
      m_routingManager(routingManager),
      m_transactionManager(transactionManager),
      m_peerStorage(peerStorage),
      m_tokenManager(tokenManager) {
    getLogger()->info("Peer lookup manager created");
}

std::string DHTPeerLookup::startLookup(const InfoHash& infoHash, GetPeersLookupCallback callback) {
    if (!m_routingManager || !m_transactionManager) {
        getLogger()->error("Cannot start lookup: Required components not available");
        return "";
    }

    // Generate a lookup ID
    std::string lookupID = generateLookupID();

    // Create a lookup
    auto lookup = std::make_shared<PeerLookup>(lookupID, infoHash, callback);

    // Add the lookup to the map
    {
        std::lock_guard<util::CheckedMutex> lock(m_lookupsMutex);
        m_lookups[lookupID] = lookup;
    }

    // Get the closest nodes from the routing table
    auto closestNodes = m_routingManager->getClosestNodes(infoHash, DEFAULT_LOOKUP_MAX_RESULTS);

    // Add the nodes to the lookup
    for (const auto& node : closestNodes) {
        lookup->addNode(node);
    }

    // Continue the lookup
    continueLookup(lookup);

    return lookupID;
}

bool DHTPeerLookup::announceAsPeer(const InfoHash& infoHash, uint16_t port, std::function<void(bool)> callback) {
    if (!m_routingManager || !m_transactionManager || !m_tokenManager) {
        getLogger()->error("Cannot announce as peer: Required components not available");
        return false;
    }

    // Start a get_peers lookup to find nodes and get tokens
    auto lookupCallback = [this, infoHash, port, callback](
        const std::vector<network::EndPoint>& peers,
        const std::vector<std::shared_ptr<Node>>& nodes,
        const std::string& token) {

        // If we found peers, store them
        if (!peers.empty() && m_peerStorage) {
            for (const auto& peer : peers) {
                m_peerStorage->storePeer(infoHash, peer);
            }
        }

        // If we didn't get any nodes or tokens, the announcement fails
        if (nodes.empty() || token.empty()) {
            getLogger()->error("Cannot announce as peer: No nodes or tokens found");
            completeAnnouncement(infoHash, false, callback);
            return;
        }

        // Generate an announcement ID
        std::string announcementID = generateLookupID();

        // Create an announcement
        auto announcement = std::make_shared<std::tuple<InfoHash, uint16_t, std::function<void(bool)>>>(
            infoHash, port, callback
        );

        // Add the announcement to the map
        {
            std::lock_guard<util::CheckedMutex> lock(m_announcementsMutex);
            m_announcements[announcementID] = announcement;
        }

        // Announce to the closest node
        auto closestNode = nodes[0];

        // Create an announce_peer query
        auto query = std::make_shared<AnnouncePeerQuery>(infoHash, port);
        query->setNodeID(m_nodeID);
        query->setToken(token);

        // Create a transaction
        std::string transactionID = m_transactionManager->createTransaction(
            query,
            closestNode->getEndpoint(),
            [this, closestNode](std::shared_ptr<ResponseMessage> response) {
                // Handle the response
                handleAnnouncePeerResponse(response, closestNode->getEndpoint());
            },
            [this, closestNode](std::shared_ptr<ErrorMessage> error) {
                // Handle the error
                handleAnnouncePeerError(error, closestNode->getEndpoint());
            },
            [this, transactionID = std::string()]() {
                // Handle the timeout
                if (!transactionID.empty()) {
                    handleAnnouncePeerTimeout(transactionID);
                }
            }
        );

        if (transactionID.empty()) {
            getLogger()->error("Failed to create transaction for announce_peer to {}",
                         closestNode->getEndpoint().toString());
            completeAnnouncement(infoHash, false, callback);
            return;
        }

        // Add the transaction to the map
        {
            std::lock_guard<util::CheckedMutex> lock(m_announcementsMutex);
            m_transactionToAnnouncement[transactionID] = announcementID;
        }

        // Send the query
        if (!m_transactionManager->findTransaction(transactionID)) {
            getLogger()->error("Failed to find transaction {} for announce_peer to {}",
                         transactionID, closestNode->getEndpoint().toString());
            completeAnnouncement(infoHash, false, callback);
            return;
        }

        getLogger()->debug("Sent announce_peer for {} to {}", infoHashToString(infoHash),
                     closestNode->getEndpoint().toString());
    };

    // Start the lookup
    std::string lookupID = startLookup(infoHash, lookupCallback);

    if (lookupID.empty()) {
        getLogger()->error("Failed to start get_peers lookup for announce_peer");
        completeAnnouncement(infoHash, false, callback);
        return false;
    }

    return true;
}

bool DHTPeerLookup::handleGetPeersResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender) {
    if (!response) {
        getLogger()->error("Invalid response");
        return false;
    }

    // Find the lookup
    auto lookup = findLookupByTransactionID(response->getTransactionID());
    if (!lookup) {
        getLogger()->warning("Cannot handle get_peers response: Lookup not found for transaction {}",
                      response->getTransactionID());
        return false;
    }

    // Get the sender's node ID
    // Mark the node as responded
    auto nodeID = response->getNodeID();
    if (nodeID.has_value()) {
        lookup->markNodeAsResponded(nodeID.value());
    }

    // Get the token from the response
    auto getPeersResponse = std::dynamic_pointer_cast<GetPeersResponse>(response);
    if (!getPeersResponse) {
        getLogger()->error("Failed to cast response to GetPeersResponse");
        return false;
    }

    auto token = getPeersResponse->getToken();
    if (token.has_value()) {
        auto senderNodeID = getPeersResponse->getNodeID();
        if (senderNodeID.has_value()) {
            lookup->addToken(senderNodeID.value(), token.value());
        }
    }

    // Get the peers from the response
    auto peers = getPeersResponse->getPeers();
    if (peers.has_value() && !peers.value().empty()) {
        getLogger()->debug("Get_peers response from {} has {} peers", sender.toString(), peers.value().size());

        // Add the peers to the lookup
        if (peers.has_value()) {
            for (const auto& peer : peers.value()) {
                lookup->addPeer(peer);
            }
        }

        // Store the peers if we have a peer storage
        if (m_peerStorage) {
            if (peers.has_value()) {
                for (const auto& peer : peers.value()) {
                    m_peerStorage->storePeer(lookup->getInfoHash(), peer);
                }
            }
        }
    } else {
        getLogger()->debug("Get_peers response from {} has no peers", sender.toString());
    }

    // Get the nodes from the response
    const auto& nodes = getPeersResponse->getNodes();
    if (!nodes.empty()) {
        getLogger()->debug("Get_peers response from {} has {} nodes", sender.toString(), nodes.size());

        // Add the nodes to the lookup
        for (const auto& node : nodes) {
            lookup->addNode(node);
        }
    } else {
        getLogger()->debug("Get_peers response from {} has no nodes", sender.toString());
    }

    // Continue the lookup
    continueLookup(lookup);

    return true;
}

bool DHTPeerLookup::handleGetPeersError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender) {
    if (!error) {
        getLogger()->error("Invalid error");
        return false;
    }

    // Find the lookup
    auto lookup = findLookupByTransactionID(error->getTransactionID());
    if (!lookup) {
        getLogger()->warning("Cannot handle get_peers error: Lookup not found for transaction {}",
                      error->getTransactionID());
        return false;
    }

    // We don't have the node ID in the error message, so we need to look it up in the transaction
    // For now, just log the error and continue
    getLogger()->warning("Get_peers error from {}: {} ({})",
                   sender.toString(),
                   error->getMessage(),
                   error->getCode());

    // Continue the lookup
    continueLookup(lookup);

    return true;
}

bool DHTPeerLookup::handleGetPeersTimeout(const std::string& transactionID) {
    // Find the lookup
    auto lookup = findLookupByTransactionID(transactionID);
    if (!lookup) {
        getLogger()->warning("Cannot handle get_peers timeout: Lookup not found for transaction {}", transactionID);
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

bool DHTPeerLookup::handleAnnouncePeerResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender) {
    if (!response) {
        getLogger()->error("Invalid response");
        return false;
    }

    // Find the announcement
    auto announcement = findAnnouncementByTransactionID(response->getTransactionID());
    if (!announcement) {
        getLogger()->warning("Cannot handle announce_peer response: Announcement not found for transaction {}",
                      response->getTransactionID());
        return false;
    }

    // Get the announcement details
    auto [infoHash, port, callback] = *announcement;

    // Log the response
    getLogger()->debug("Received announce_peer response from {} for {}", sender.toString(),
                 infoHashToString(infoHash));

    // Complete the announcement
    completeAnnouncement(infoHash, true, callback);

    return true;
}

bool DHTPeerLookup::handleAnnouncePeerError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender) {
    if (!error) {
        getLogger()->error("Invalid error");
        return false;
    }

    // Find the announcement
    auto announcement = findAnnouncementByTransactionID(error->getTransactionID());
    if (!announcement) {
        getLogger()->warning("Cannot handle announce_peer error: Announcement not found for transaction {}",
                      error->getTransactionID());
        return false;
    }

    // Get the announcement details
    auto [infoHash, port, callback] = *announcement;

    // Log the error
    getLogger()->warning("Received announce_peer error from {} for {}: {} ({})",
                   sender.toString(), infoHashToString(infoHash), error->getMessage(), error->getCode());

    // Complete the announcement
    completeAnnouncement(infoHash, false, callback);

    return true;
}

bool DHTPeerLookup::handleAnnouncePeerTimeout(const std::string& transactionID) {
    // Find the announcement
    auto announcement = findAnnouncementByTransactionID(transactionID);
    if (!announcement) {
        getLogger()->warning("Cannot handle announce_peer timeout: Announcement not found for transaction {}", transactionID);
        return false;
    }

    // Get the announcement details
    auto [infoHash, port, callback] = *announcement;

    // Log the timeout
    getLogger()->warning("Announce_peer timeout for {}", infoHashToString(infoHash));

    // Complete the announcement
    completeAnnouncement(infoHash, false, callback);

    return true;
}

void DHTPeerLookup::continueLookup(std::shared_ptr<PeerLookup> lookup) {
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
        lookup->markNodeAsQueried(node->getID());

        // Create a get_peers query
        auto query = std::make_shared<GetPeersQuery>(lookup->getInfoHash());
        query->setNodeID(m_nodeID);

        // Create a transaction
        std::string transactionID = m_transactionManager->createTransaction(
            query,
            node->getEndpoint(),
            [this, node](std::shared_ptr<ResponseMessage> response) {
                // Handle the response
                handleGetPeersResponse(response, node->getEndpoint());
            },
            [this, node](std::shared_ptr<ErrorMessage> error) {
                // Handle the error
                handleGetPeersError(error, node->getEndpoint());
            },
            [this, transactionID = std::string()]() {
                // Handle the timeout
                if (!transactionID.empty()) {
                    handleGetPeersTimeout(transactionID);
                }
            }
        );

        if (transactionID.empty()) {
            getLogger()->error("Failed to create transaction for get_peers to {}", node->getEndpoint().toString());
            continue;
        }

        // Add the transaction to the map
        {
            std::lock_guard<util::CheckedMutex> lock(m_lookupsMutex);
            m_transactionToLookup[transactionID] = lookup->getID();
        }

        // Send the query
        if (!m_transactionManager->findTransaction(transactionID)) {
            getLogger()->error("Failed to find transaction {} for get_peers to {}",
                         transactionID, node->getEndpoint().toString());
            continue;
        }
    }
}

void DHTPeerLookup::completeAnnouncement(const InfoHash& infoHash, bool success, std::function<void(bool)> callback) {
    // Remove all announcements for this info hash
    {
        std::lock_guard<util::CheckedMutex> lock(m_announcementsMutex);

        // Find all announcements for this info hash
        std::vector<std::string> announcementIDs;
        for (const auto& entry : m_announcements) {
            auto [announcementInfoHash, port, announcementCallback] = *entry.second;
            if (announcementInfoHash == infoHash) {
                announcementIDs.push_back(entry.first);
            }
        }

        // Remove the announcements
        for (const auto& announcementID : announcementIDs) {
            m_announcements.erase(announcementID);
        }

        // Remove all transactions for these announcements
        std::vector<std::string> transactionIDs;
        for (const auto& entry : m_transactionToAnnouncement) {
            if (std::find(announcementIDs.begin(), announcementIDs.end(), entry.second) != announcementIDs.end()) {
                transactionIDs.push_back(entry.first);
            }
        }

        for (const auto& transactionID : transactionIDs) {
            m_transactionToAnnouncement.erase(transactionID);
        }
    }

    // Call the callback
    if (callback) {
        callback(success);
    }
}

std::string DHTPeerLookup::generateLookupID() const {
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

std::shared_ptr<PeerLookup> DHTPeerLookup::findLookupByTransactionID(const std::string& transactionID) {
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

std::shared_ptr<std::tuple<InfoHash, uint16_t, std::function<void(bool)>>> DHTPeerLookup::findAnnouncementByTransactionID(const std::string& transactionID) {
    std::lock_guard<util::CheckedMutex> lock(m_announcementsMutex);

    auto it = m_transactionToAnnouncement.find(transactionID);
    if (it == m_transactionToAnnouncement.end()) {
        return nullptr;
    }

    auto announcementIt = m_announcements.find(it->second);
    if (announcementIt == m_announcements.end()) {
        return nullptr;
    }

    return announcementIt->second;
}

} // namespace dht_hunter::dht
