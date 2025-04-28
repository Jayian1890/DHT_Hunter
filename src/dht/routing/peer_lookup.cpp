#include "dht_hunter/dht/routing/peer_lookup.hpp"
#include "dht_hunter/dht/core/routing_table.hpp"
#include "dht_hunter/dht/transactions/transaction_manager.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include "dht_hunter/dht/storage/token_manager.hpp"
#include "dht_hunter/dht/storage/peer_storage.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/utils/lock_utils.hpp"
#include <algorithm>
#include <random>

namespace dht_hunter::dht {

// Initialize static members
std::shared_ptr<PeerLookup> PeerLookup::s_instance = nullptr;
std::mutex PeerLookup::s_instanceMutex;

std::shared_ptr<PeerLookup> PeerLookup::getInstance(
    const DHTConfig& config,
    const NodeID& nodeID,
    std::shared_ptr<RoutingTable> routingTable,
    std::shared_ptr<TransactionManager> transactionManager,
    std::shared_ptr<MessageSender> messageSender,
    std::shared_ptr<TokenManager> tokenManager,
    std::shared_ptr<PeerStorage> peerStorage) {

    try {
        return utils::withLock(s_instanceMutex, [&]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<PeerLookup>(new PeerLookup(
                    config, nodeID, routingTable, transactionManager, messageSender, tokenManager, peerStorage));
            }
            return s_instance;
        }, "PeerLookup::s_instanceMutex");
    } catch (const utils::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
        return nullptr;
    }
}

PeerLookup::PeerLookup(const DHTConfig& config,
                     const NodeID& nodeID,
                     std::shared_ptr<RoutingTable> routingTable,
                     std::shared_ptr<TransactionManager> transactionManager,
                     std::shared_ptr<MessageSender> messageSender,
                     std::shared_ptr<TokenManager> tokenManager,
                     std::shared_ptr<PeerStorage> peerStorage)
    : m_config(config),
      m_nodeID(nodeID),
      m_routingTable(routingTable),
      m_transactionManager(transactionManager),
      m_messageSender(messageSender),
      m_tokenManager(tokenManager),
      m_peerStorage(peerStorage) {
}

PeerLookup::~PeerLookup() {
    // Clear the singleton instance
    try {
        utils::withLock(s_instanceMutex, [this]() {
            if (s_instance.get() == this) {
                s_instance.reset();
            }
        }, "PeerLookup::s_instanceMutex");
    } catch (const utils::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::lookup(const InfoHash& infoHash, std::function<void(const std::vector<network::EndPoint>&)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Generate a lookup ID
    std::string lookupID = infoHashToString(infoHash);

    // Check if a lookup with this ID already exists
    if (m_lookups.find(lookupID) != m_lookups.end()) {
        return;
    }

    // Create a new lookup
    Lookup lookup(infoHash, callback);

    // Get the closest nodes from the routing table
    if (m_routingTable) {
        lookup.nodes = m_routingTable->getClosestNodes(NodeID(infoHash), m_config.getMaxResults());
    }

    // Add the lookup
    m_lookups.emplace(lookupID, lookup);

    // Send queries
    sendQueries(lookupID);
}

void PeerLookup::announce(const InfoHash& infoHash, uint16_t port, std::function<void(bool)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Generate a lookup ID
    std::string lookupID = infoHashToString(infoHash);

    // Check if a lookup with this ID already exists
    if (m_lookups.find(lookupID) != m_lookups.end()) {
        return;
    }

    // Create a new lookup
    Lookup lookup(infoHash, port, callback);

    // Get the closest nodes from the routing table
    if (m_routingTable) {
        lookup.nodes = m_routingTable->getClosestNodes(NodeID(infoHash), m_config.getMaxResults());
    }

    // Add the lookup
    m_lookups.emplace(lookupID, lookup);

    // Send queries
    sendQueries(lookupID);
}

void PeerLookup::sendQueries(const std::string& lookupID) {
    try {
        utils::withLock(m_mutex, [this, &lookupID]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            Lookup& lookup = it->second;

            // Check if we've reached the maximum number of iterations
            if (lookup.iteration >= LOOKUP_MAX_ITERATIONS) {
                if (lookup.announcing) {
                    announceToNodes(lookupID);
                } else {
                    completeLookup(lookupID);
                }
                return;
            }

            // Increment the iteration counter
            lookup.iteration++;

            // Sort the nodes by distance to the target
            std::sort(lookup.nodes.begin(), lookup.nodes.end(),
                [&lookup](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
                    NodeID targetID(lookup.infoHash);
                    NodeID distanceA = a->getID().distanceTo(targetID);
                    NodeID distanceB = b->getID().distanceTo(targetID);
                    return distanceA < distanceB;
                });

            // Count the number of queries we need to send
            size_t queriesToSend = 0;
            for (const auto& node : lookup.nodes) {
                // Skip nodes we've already queried
                if (lookup.queriedNodes.find(nodeIDToString(node->getID())) != lookup.queriedNodes.end()) {
                    continue;
                }

                // Skip nodes that are already being queried
                if (lookup.activeQueries.find(nodeIDToString(node->getID())) != lookup.activeQueries.end()) {
                    continue;
                }

                queriesToSend++;

                // Stop if we've reached the alpha value
                if (queriesToSend >= m_config.getAlpha()) {
                    break;
                }
            }

            // If there are no queries to send, check if the lookup is complete
            if (queriesToSend == 0) {
                if (isLookupComplete(lookupID)) {
                    if (lookup.announcing) {
                        announceToNodes(lookupID);
                    } else {
                        completeLookup(lookupID);
                    }
                }
                return;
            }

            // Send queries
            size_t queriesSent = 0;
            for (const auto& node : lookup.nodes) {
                // Skip nodes we've already queried
                std::string nodeIDStr = nodeIDToString(node->getID());
                if (lookup.queriedNodes.find(nodeIDStr) != lookup.queriedNodes.end()) {
                    continue;
                }

                // Skip nodes that are already being queried
                if (lookup.activeQueries.find(nodeIDStr) != lookup.activeQueries.end()) {
                    continue;
                }

                // Create a get_peers query
                auto query = std::make_shared<GetPeersQuery>("", m_nodeID, lookup.infoHash);

                // Send the query
                if (m_transactionManager && m_messageSender) {
                    std::string transactionID = m_transactionManager->createTransaction(
                        query,
                        node->getEndpoint(),
                        [this, lookupID, nodeIDStr](std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender) {
                            auto getPeersResponse = std::dynamic_pointer_cast<GetPeersResponse>(response);
                            if (getPeersResponse) {
                                handleResponse(lookupID, getPeersResponse, sender);
                            }
                        },
                        [this, lookupID, nodeIDStr](std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender) {
                            handleError(lookupID, error, sender);
                        },
                        [this, lookupID, node]() {
                            handleTimeout(lookupID, node->getID());
                        });

                    if (!transactionID.empty()) {
                        // Add the node to the queried nodes
                        lookup.queriedNodes.insert(nodeIDStr);

                        // Add the node to the active queries
                        lookup.activeQueries.insert(nodeIDStr);

                        // Send the query
                        m_messageSender->sendQuery(query, node->getEndpoint());

                        queriesSent++;

                        // Stop if we've sent enough queries
                        if (queriesSent >= m_config.getAlpha()) {
                            break;
                        }
                    }
                }
            }
        }, "PeerLookup::m_mutex");
    } catch (const utils::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::handleResponse(const std::string& lookupID, std::shared_ptr<GetPeersResponse> response, const network::EndPoint& /*sender*/) {
    // Variables to store data we'll need after releasing the lock
    std::vector<std::shared_ptr<Node>> nodesToAdd;
    std::vector<network::EndPoint> peersToAdd;
    InfoHash infoHashCopy;
    bool shouldSendQueries = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Find the lookup
        auto it = m_lookups.find(lookupID);
        if (it == m_lookups.end()) {
            return;
        }

        Lookup& lookup = it->second;
        infoHashCopy = lookup.infoHash; // Copy the info hash for use outside the lock

        // Get the node ID from the response
        if (!response->getNodeID()) {
            return;
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

                    // Store the peer for adding to the peer storage after releasing the lock
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

                    // Store the node for adding to the routing table after releasing the lock
                    nodesToAdd.push_back(node);
                }
            }
        }

        // Set flag to send more queries after releasing the lock
        shouldSendQueries = true;
    } // Release the lock

    // Add peers to the peer storage outside the lock
    if (m_peerStorage && !peersToAdd.empty()) {
        for (const auto& peer : peersToAdd) {
            m_peerStorage->addPeer(infoHashCopy, peer);
        }
    }

    // Add nodes to the routing table outside the lock
    if (m_routingTable && !nodesToAdd.empty()) {
        for (const auto& node : nodesToAdd) {
            m_routingTable->addNode(node);
        }
    }

    // Send more queries if needed
    if (shouldSendQueries) {
        sendQueries(lookupID);
    }
}

void PeerLookup::handleError(const std::string& lookupID, std::shared_ptr<ErrorMessage> /*error*/, const network::EndPoint& sender) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find the lookup
    auto it = m_lookups.find(lookupID);
    if (it == m_lookups.end()) {
        return;
    }

    Lookup& lookup = it->second;

    // Find the node
    auto nodeIt = std::find_if(lookup.nodes.begin(), lookup.nodes.end(),
        [&sender](const std::shared_ptr<Node>& node) {
            return node->getEndpoint() == sender;
        });

    if (nodeIt == lookup.nodes.end()) {
        return;
    }

    std::string nodeIDStr = nodeIDToString((*nodeIt)->getID());

    // Remove the node from the active queries
    lookup.activeQueries.erase(nodeIDStr);

    // Send more queries
    sendQueries(lookupID);
}

void PeerLookup::handleTimeout(const std::string& lookupID, const NodeID& nodeID) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find the lookup
    auto it = m_lookups.find(lookupID);
    if (it == m_lookups.end()) {
        return;
    }

    Lookup& lookup = it->second;

    std::string nodeIDStr = nodeIDToString(nodeID);

    // Remove the node from the active queries
    lookup.activeQueries.erase(nodeIDStr);

    // Send more queries
    sendQueries(lookupID);
}

bool PeerLookup::isLookupComplete(const std::string& lookupID) {
    // Find the lookup
    auto it = m_lookups.find(lookupID);
    if (it == m_lookups.end()) {
        return true;
    }

    const Lookup& lookup = it->second;

    // If there are active queries, the lookup is not complete
    if (!lookup.activeQueries.empty()) {
        return false;
    }

    // If we haven't queried any nodes, the lookup is not complete
    if (lookup.queriedNodes.empty()) {
        return false;
    }

    // If we've found enough peers, the lookup is complete
    if (lookup.peers.size() >= m_config.getMaxResults()) {
        return true;
    }

    // If we've queried all nodes, the lookup is complete
    if (lookup.queriedNodes.size() >= lookup.nodes.size()) {
        return true;
    }

    // Sort the nodes by distance to the target
    std::vector<std::shared_ptr<Node>> sortedNodes = lookup.nodes;
    std::sort(sortedNodes.begin(), sortedNodes.end(),
        [&lookup](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
            NodeID targetID(lookup.infoHash);
            NodeID distanceA = a->getID().distanceTo(targetID);
            NodeID distanceB = b->getID().distanceTo(targetID);
            return distanceA < distanceB;
        });

    // Check if we've queried the k closest nodes
    size_t k = std::min(m_config.getKBucketSize(), sortedNodes.size());
    for (size_t i = 0; i < k; ++i) {
        std::string nodeIDStr = nodeIDToString(sortedNodes[i]->getID());
        if (lookup.queriedNodes.find(nodeIDStr) == lookup.queriedNodes.end()) {
            return false;
        }
    }

    return true;
}

void PeerLookup::completeLookup(const std::string& lookupID) {
    // Find the lookup
    auto it = m_lookups.find(lookupID);
    if (it == m_lookups.end()) {
        return;
    }

    Lookup& lookup = it->second;

    // Call the callback
    if (lookup.lookupCallback) {
        lookup.lookupCallback(lookup.peers);
    }

    // Remove the lookup
    m_lookups.erase(it);
}

void PeerLookup::announceToNodes(const std::string& lookupID) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find the lookup
    auto it = m_lookups.find(lookupID);
    if (it == m_lookups.end()) {
        return;
    }

    Lookup& lookup = it->second;

    // Check if there are any nodes to announce to
    if (lookup.nodeTokens.empty()) {
        completeAnnouncement(lookupID, false);
        return;
    }

    // Count the number of announcements we need to send
    size_t announcementsToSend = 0;
    for (const auto& [nodeIDStr, token] : lookup.nodeTokens) {
        // Skip nodes we've already announced to
        if (lookup.announcedNodes.find(nodeIDStr) != lookup.announcedNodes.end()) {
            continue;
        }

        // Skip nodes that are already being announced to
        if (lookup.activeAnnouncements.find(nodeIDStr) != lookup.activeAnnouncements.end()) {
            continue;
        }

        announcementsToSend++;
    }

    // If there are no announcements to send, complete the announcement
    if (announcementsToSend == 0) {
        completeAnnouncement(lookupID, !lookup.announcedNodes.empty());
        return;
    }

    // Send announcements
    size_t announcementsSent = 0;
    for (const auto& [nodeIDStr, token] : lookup.nodeTokens) {
        // Skip nodes we've already announced to
        if (lookup.announcedNodes.find(nodeIDStr) != lookup.announcedNodes.end()) {
            continue;
        }

        // Skip nodes that are already being announced to
        if (lookup.activeAnnouncements.find(nodeIDStr) != lookup.activeAnnouncements.end()) {
            continue;
        }

        // Find the node
        auto nodeIt = std::find_if(lookup.nodes.begin(), lookup.nodes.end(),
            [&nodeIDStr](const std::shared_ptr<Node>& node) {
                return nodeIDToString(node->getID()) == nodeIDStr;
            });

        if (nodeIt == lookup.nodes.end()) {
            continue;
        }

        // Create an announce_peer query
        auto query = std::make_shared<AnnouncePeerQuery>("", m_nodeID, lookup.infoHash, lookup.port, token);

        // Send the query
        if (m_transactionManager && m_messageSender) {
            std::string transactionID = m_transactionManager->createTransaction(
                query,
                (*nodeIt)->getEndpoint(),
                [this, lookupID, nodeIDStr](std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender) {
                    auto announcePeerResponse = std::dynamic_pointer_cast<AnnouncePeerResponse>(response);
                    if (announcePeerResponse) {
                        handleAnnounceResponse(lookupID, announcePeerResponse, sender);
                    }
                },
                [this, lookupID, nodeIDStr](std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender) {
                    handleAnnounceError(lookupID, error, sender);
                },
                [this, lookupID, nodeIDStr]() {
                    // Convert the string to a NodeID
                    NodeID nodeID;
                    if (nodeIDStr.size() == 20) {
                        std::copy(nodeIDStr.begin(), nodeIDStr.end(), nodeID.begin());
                        handleAnnounceTimeout(lookupID, nodeID);
                    } else {
                    }
                });

            if (!transactionID.empty()) {
                // Add the node to the active announcements
                lookup.activeAnnouncements.insert(nodeIDStr);

                // Send the query
                m_messageSender->sendQuery(query, (*nodeIt)->getEndpoint());

                announcementsSent++;
            }
        }
    }

    // If we didn't send any announcements, complete the announcement
    if (announcementsSent == 0) {
        completeAnnouncement(lookupID, false);
    }
}

void PeerLookup::handleAnnounceResponse(const std::string& lookupID, std::shared_ptr<AnnouncePeerResponse> response, const network::EndPoint& /*sender*/) {
    bool shouldCompleteAnnouncement = false;
    bool success = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Find the lookup
        auto it = m_lookups.find(lookupID);
        if (it == m_lookups.end()) {
            return;
        }

        Lookup& lookup = it->second;

        // Get the node ID from the response
        if (!response->getNodeID()) {
            return;
        }

        std::string nodeIDStr = nodeIDToString(response->getNodeID().value());

        // Remove the node from the active announcements
        lookup.activeAnnouncements.erase(nodeIDStr);

        // Add the node to the announced nodes
        lookup.announcedNodes.insert(nodeIDStr);

        // Check if we've announced to all nodes
        if (lookup.activeAnnouncements.empty() && lookup.announcedNodes.size() >= lookup.nodeTokens.size()) {
            shouldCompleteAnnouncement = true;
            success = true;
        }
    } // Release the lock

    // Complete the announcement outside the lock if needed
    if (shouldCompleteAnnouncement) {
        completeAnnouncement(lookupID, success);
    }
}

void PeerLookup::handleAnnounceError(const std::string& lookupID, std::shared_ptr<ErrorMessage> /*error*/, const network::EndPoint& sender) {
    bool shouldCompleteAnnouncement = false;
    bool success = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Find the lookup
        auto it = m_lookups.find(lookupID);
        if (it == m_lookups.end()) {
            return;
        }

        Lookup& lookup = it->second;

        // Find the node
        auto nodeIt = std::find_if(lookup.nodes.begin(), lookup.nodes.end(),
            [&sender](const std::shared_ptr<Node>& node) {
                return node->getEndpoint() == sender;
            });

        if (nodeIt == lookup.nodes.end()) {
            return;
        }

        std::string nodeIDStr = nodeIDToString((*nodeIt)->getID());

        // Remove the node from the active announcements
        lookup.activeAnnouncements.erase(nodeIDStr);

        // Check if we've announced to all nodes
        if (lookup.activeAnnouncements.empty()) {
            shouldCompleteAnnouncement = true;
            success = !lookup.announcedNodes.empty();
        }
    } // Release the lock

    // Complete the announcement outside the lock if needed
    if (shouldCompleteAnnouncement) {
        completeAnnouncement(lookupID, success);
    }
}

void PeerLookup::handleAnnounceTimeout(const std::string& lookupID, const NodeID& nodeID) {
    // Convert the NodeID to a string
    std::string nodeIDStr = nodeIDToString(nodeID);
    bool shouldCompleteAnnouncement = false;
    bool success = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Find the lookup
        auto it = m_lookups.find(lookupID);
        if (it == m_lookups.end()) {
            return;
        }

        Lookup& lookup = it->second;

        // Remove the node from the active announcements
        lookup.activeAnnouncements.erase(nodeIDStr);

        // Check if we've announced to all nodes
        if (lookup.activeAnnouncements.empty()) {
            shouldCompleteAnnouncement = true;
            success = !lookup.announcedNodes.empty();
        }
    } // Release the lock

    // Complete the announcement outside the lock if needed
    if (shouldCompleteAnnouncement) {
        completeAnnouncement(lookupID, success);
    }
}

void PeerLookup::completeAnnouncement(const std::string& lookupID, bool success) {
    // Store the callback to call after releasing the lock
    std::function<void(bool)> callback;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Find the lookup
        auto it = m_lookups.find(lookupID);
        if (it == m_lookups.end()) {
            return;
        }

        Lookup& lookup = it->second;

        // Store the callback
        callback = lookup.announceCallback;

        // Remove the lookup
        m_lookups.erase(it);
    } // Release the lock

    // Call the callback outside the lock
    if (callback) {
        callback(success);
    }
}

} // namespace dht_hunter::dht
