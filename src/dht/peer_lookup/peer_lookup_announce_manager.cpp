#include "dht_hunter/dht/peer_lookup/peer_lookup_announce_manager.hpp"
#include "dht_hunter/dht/peer_lookup/peer_lookup.hpp"
#include "dht_hunter/dht/transactions/transaction_manager.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"
#include <algorithm>

namespace dht_hunter::dht {

PeerLookupAnnounceManager::PeerLookupAnnounceManager(const DHTConfig& config, 
                                                   const NodeID& nodeID,
                                                   std::shared_ptr<TransactionManager> transactionManager,
                                                   std::shared_ptr<MessageSender> messageSender)
    : m_config(config),
      m_nodeID(nodeID),
      m_transactionManager(transactionManager),
      m_messageSender(messageSender) {
}

bool PeerLookupAnnounceManager::announceToNodes(
    const std::string& /*lookupID*/, 
    PeerLookupState& lookup,
    std::function<void(std::shared_ptr<ResponseMessage>, const network::EndPoint&)> responseCallback,
    std::function<void(std::shared_ptr<ErrorMessage>, const network::EndPoint&)> errorCallback,
    std::function<void(const std::string&)> timeoutCallback) {
    
    // Check if there are any nodes to announce to
    if (lookup.nodeTokens.empty()) {
        return false;
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
                responseCallback,
                errorCallback,
                [timeoutCallback, nodeIDStr]() {
                    timeoutCallback(nodeIDStr);
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

    return announcementsSent > 0;
}

void PeerLookupAnnounceManager::completeAnnouncement(PeerLookupState& lookup, bool success) const {
    // Call the callback
    if (lookup.announceCallback) {
        lookup.announceCallback(success);
    }
}

} // namespace dht_hunter::dht
