#pragma once

#include "utils/dht_core_utils.hpp"
#include "dht_hunter/dht/types.hpp"
#include "dht_hunter/dht/network/response_message.hpp"
#include "dht_hunter/dht/network/error_message.hpp"
#include <memory>
#include <string>
#include <functional>

namespace dht_hunter::dht {

using dht_hunter::utils::dht_core::DHTConfig;

// Forward declarations
class TransactionManager;
class MessageSender;
struct PeerLookupState;

/**
 * @brief Manages peer announcements
 */
class PeerLookupAnnounceManager {
public:
    /**
     * @brief Constructor
     * @param config The DHT configuration
     * @param nodeID The node ID
     * @param transactionManager The transaction manager
     * @param messageSender The message sender
     */
    PeerLookupAnnounceManager(const DHTConfig& config,
                             const NodeID& nodeID,
                             std::shared_ptr<TransactionManager> transactionManager,
                             std::shared_ptr<MessageSender> messageSender);

    /**
     * @brief Announces a peer to nodes with tokens
     * @param lookupID The lookup ID
     * @param lookup The lookup state
     * @param responseCallback Callback for handling responses
     * @param errorCallback Callback for handling errors
     * @param timeoutCallback Callback for handling timeouts
     * @return True if announcements were sent, false otherwise
     */
    bool announceToNodes(
        const std::string& lookupID,
        PeerLookupState& lookup,
        std::function<void(std::shared_ptr<ResponseMessage>, const EndPoint&)> responseCallback,
        std::function<void(std::shared_ptr<ErrorMessage>, const EndPoint&)> errorCallback,
        std::function<void(const std::string&)> timeoutCallback);

    /**
     * @brief Completes an announcement
     * @param lookup The lookup state
     * @param success Whether the announcement was successful
     */
    void completeAnnouncement(PeerLookupState& lookup, bool success) const;

private:
    DHTConfig m_config;
    NodeID m_nodeID;
    std::shared_ptr<TransactionManager> m_transactionManager;
    std::shared_ptr<MessageSender> m_messageSender;
};

} // namespace dht_hunter::dht
