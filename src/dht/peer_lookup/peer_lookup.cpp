#include "dht_hunter/dht/peer_lookup/peer_lookup.hpp"
#include "dht_hunter/dht/peer_lookup/peer_lookup_query_manager.hpp"
#include "dht_hunter/dht/peer_lookup/peer_lookup_announce_manager.hpp"
#include "dht_hunter/dht/peer_lookup/peer_lookup_response_handler.hpp"
#include "dht_hunter/dht/peer_lookup/peer_lookup_utils.hpp"
#include "dht_hunter/dht/core/routing_table.hpp"
#include "dht_hunter/dht/transactions/transaction_manager.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include "dht_hunter/dht/storage/token_manager.hpp"
#include "dht_hunter/dht/storage/peer_storage.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"
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
        return utility::thread::withLock(s_instanceMutex, [&]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<PeerLookup>(new PeerLookup(
                    config, nodeID, routingTable, transactionManager, messageSender, tokenManager, peerStorage));
            }
            return s_instance;
        }, "PeerLookup::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
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
        utility::thread::withLock(s_instanceMutex, [this]() {
            if (s_instance.get() == this) {
                s_instance.reset();
            }
        }, "PeerLookup::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::lookup(const InfoHash& infoHash, std::function<void(const std::vector<network::EndPoint>&)> callback) {
    try {
        utility::thread::withLock(m_mutex, [this, &infoHash, &callback]() {
            // Generate a lookup ID
            std::string lookupID = peer_lookup_utils::generateLookupID(infoHash);

            // Check if a lookup with this ID already exists
            if (m_lookups.find(lookupID) != m_lookups.end()) {
                return;
            }

            // Create a new lookup
            PeerLookupState lookup(infoHash, callback);

            // Get the closest nodes from the routing table
            if (m_routingTable) {
                lookup.nodes = m_routingTable->getClosestNodes(NodeID(infoHash), m_config.getMaxResults());
            }

            // Add the lookup
            m_lookups.emplace(lookupID, lookup);

            // Send queries
            sendQueries(lookupID);
        }, "PeerLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::announce(const InfoHash& infoHash, uint16_t port, std::function<void(bool)> callback) {
    try {
        utility::thread::withLock(m_mutex, [this, &infoHash, port, &callback]() {
            // Generate a lookup ID
            std::string lookupID = peer_lookup_utils::generateLookupID(infoHash);

            // Check if a lookup with this ID already exists
            if (m_lookups.find(lookupID) != m_lookups.end()) {
                return;
            }

            // Create a new lookup
            PeerLookupState lookup(infoHash, port, callback);

            // Get the closest nodes from the routing table
            if (m_routingTable) {
                lookup.nodes = m_routingTable->getClosestNodes(NodeID(infoHash), m_config.getMaxResults());
            }

            // Add the lookup
            m_lookups.emplace(lookupID, lookup);

            // Send queries
            sendQueries(lookupID);
        }, "PeerLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::sendQueries(const std::string& lookupID) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            PeerLookupState& lookup = it->second;

            // Create query manager and response handler
            PeerLookupQueryManager queryManager(m_config, m_nodeID, m_routingTable, m_transactionManager, m_messageSender);
            PeerLookupResponseHandler responseHandler(m_config, m_nodeID, m_routingTable, m_peerStorage);

            // Check if we've reached the maximum number of iterations
            if (lookup.iteration >= LOOKUP_MAX_ITERATIONS) {
                if (lookup.announcing) {
                    announceToNodes(lookupID);
                } else {
                    completeLookup(lookupID);
                }
                return;
            }

            // Check if the lookup is complete
            if (queryManager.isLookupComplete(lookup, m_config.getKBucketSize())) {
                if (lookup.announcing) {
                    announceToNodes(lookupID);
                } else {
                    completeLookup(lookupID);
                }
                return;
            }

            // Send queries using the query manager
            bool queriesSent = queryManager.sendQueries(
                lookupID,
                lookup,
                [this, lookupID](std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender) {
                    auto getPeersResponse = std::dynamic_pointer_cast<GetPeersResponse>(response);
                    if (getPeersResponse) {
                        handleResponse(lookupID, getPeersResponse, sender);
                    }
                },
                [this, lookupID](std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender) {
                    handleError(lookupID, error, sender);
                },
                [this, lookupID](const NodeID& nodeID) {
                    handleTimeout(lookupID, nodeID);
                });

            // If no queries were sent, check if the lookup is complete
            if (!queriesSent) {
                if (queryManager.isLookupComplete(lookup, m_config.getKBucketSize())) {
                    if (lookup.announcing) {
                        announceToNodes(lookupID);
                    } else {
                        completeLookup(lookupID);
                    }
                }
            }
        }, "PeerLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::handleResponse(const std::string& lookupID, std::shared_ptr<GetPeersResponse> response, const network::EndPoint& sender) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID, &response, &sender]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            PeerLookupState& lookup = it->second;

            // Create response handler
            PeerLookupResponseHandler responseHandler(m_config, m_nodeID, m_routingTable, m_peerStorage);

            // Use the response handler to process the response
            bool shouldSendQueries = responseHandler.handleResponse(lookupID, lookup, response, sender);

            // Send more queries if needed
            if (shouldSendQueries) {
                sendQueries(lookupID);
            }
        }, "PeerLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::handleError(const std::string& lookupID, std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID, &error, &sender]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            PeerLookupState& lookup = it->second;

            // Create response handler
            PeerLookupResponseHandler responseHandler(m_config, m_nodeID, m_routingTable, m_peerStorage);

            // Use the response handler to process the error
            bool shouldSendQueries = responseHandler.handleError(lookup, error, sender);

            // Send more queries if needed
            if (shouldSendQueries) {
                sendQueries(lookupID);
            }
        }, "PeerLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::handleTimeout(const std::string& lookupID, const NodeID& nodeID) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID, &nodeID]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            PeerLookupState& lookup = it->second;

            // Create response handler
            PeerLookupResponseHandler responseHandler(m_config, m_nodeID, m_routingTable, m_peerStorage);

            // Use the response handler to process the timeout
            bool shouldSendQueries = responseHandler.handleTimeout(lookup, nodeID);

            // Send more queries if needed
            if (shouldSendQueries) {
                sendQueries(lookupID);
            }
        }, "PeerLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

bool PeerLookup::isLookupComplete(const std::string& lookupID) {
    try {
        return utility::thread::withLock(m_mutex, [this, &lookupID]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return true;
            }

            const PeerLookupState& lookup = it->second;

            // Create query manager
            PeerLookupQueryManager queryManager(m_config, m_nodeID, m_routingTable, m_transactionManager, m_messageSender);

            // Use the query manager to check if the lookup is complete
            return queryManager.isLookupComplete(lookup, m_config.getKBucketSize());
        }, "PeerLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
        return true; // Assume complete on error
    }
}

void PeerLookup::completeLookup(const std::string& lookupID) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            PeerLookupState& lookup = it->second;

            // Create response handler
            PeerLookupResponseHandler responseHandler(m_config, m_nodeID, m_routingTable, m_peerStorage);

            // Use the response handler to complete the lookup
            responseHandler.completeLookup(lookup);

            // Remove the lookup
            m_lookups.erase(it);
        }, "PeerLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::announceToNodes(const std::string& lookupID) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            PeerLookupState& lookup = it->second;

            // Create announce manager
            PeerLookupAnnounceManager announceManager(m_config, m_nodeID, m_transactionManager, m_messageSender);

            // Use the announce manager to send announcements
            bool announcementsSent = announceManager.announceToNodes(
                lookupID,
                lookup,
                [this, lookupID](std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender) {
                    auto announcePeerResponse = std::dynamic_pointer_cast<AnnouncePeerResponse>(response);
                    if (announcePeerResponse) {
                        handleAnnounceResponse(lookupID, announcePeerResponse, sender);
                    }
                },
                [this, lookupID](std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender) {
                    handleAnnounceError(lookupID, error, sender);
                },
                [this, lookupID](const std::string& nodeIDStr) {
                    // Convert the string to a NodeID
                    NodeID nodeID;
                    if (nodeIDStr.size() == 20) {
                        std::copy(nodeIDStr.begin(), nodeIDStr.end(), nodeID.begin());
                        handleAnnounceTimeout(lookupID, nodeID);
                    } else {
                        // Log error
                        unified_event::logError("DHT.PeerLookup", "Invalid node ID string length: " + std::to_string(nodeIDStr.size()));
                    }
                });

            // If we didn't send any announcements, complete the announcement
            if (!announcementsSent) {
                completeAnnouncement(lookupID, false);
            }
        }, "PeerLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::handleAnnounceResponse(const std::string& lookupID, std::shared_ptr<AnnouncePeerResponse> response, const network::EndPoint& sender) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID, &response, &sender]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            PeerLookupState& lookup = it->second;

            // Create response handler
            PeerLookupResponseHandler responseHandler(m_config, m_nodeID, m_routingTable, m_peerStorage);

            // Use the response handler to process the response
            bool isComplete = responseHandler.handleAnnounceResponse(lookup, response, sender);

            // Complete the announcement if needed
            if (isComplete) {
                completeAnnouncement(lookupID, !lookup.announcedNodes.empty());
            }
        }, "PeerLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::handleAnnounceError(const std::string& lookupID, std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID, &error, &sender]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            PeerLookupState& lookup = it->second;

            // Create response handler
            PeerLookupResponseHandler responseHandler(m_config, m_nodeID, m_routingTable, m_peerStorage);

            // Use the response handler to process the error
            bool isComplete = responseHandler.handleAnnounceError(lookup, error, sender);

            // Complete the announcement if needed
            if (isComplete) {
                completeAnnouncement(lookupID, !lookup.announcedNodes.empty());
            }
        }, "PeerLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::handleAnnounceTimeout(const std::string& lookupID, const NodeID& nodeID) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID, &nodeID]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            PeerLookupState& lookup = it->second;

            // Create response handler
            PeerLookupResponseHandler responseHandler(m_config, m_nodeID, m_routingTable, m_peerStorage);

            // Use the response handler to process the timeout
            bool isComplete = responseHandler.handleAnnounceTimeout(lookup, nodeID);

            // Complete the announcement if needed
            if (isComplete) {
                completeAnnouncement(lookupID, !lookup.announcedNodes.empty());
            }
        }, "PeerLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

void PeerLookup::completeAnnouncement(const std::string& lookupID, bool success) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID, success]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            PeerLookupState& lookup = it->second;

            // Create announce manager
            PeerLookupAnnounceManager announceManager(m_config, m_nodeID, m_transactionManager, m_messageSender);

            // Use the announce manager to complete the announcement
            announceManager.completeAnnouncement(lookup, success);

            // Remove the lookup
            m_lookups.erase(it);
        }, "PeerLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerLookup", e.what());
    }
}

} // namespace dht_hunter::dht
