#pragma once

#include "utils/dht_core_utils.hpp"
#include "dht_hunter/dht/types.hpp"
#include "dht_hunter/dht/network/response_message.hpp"
#include "dht_hunter/dht/network/error_message.hpp"
#include <memory>
#include <string>
#include <functional>

namespace dht_hunter::dht {

// Forward declarations
// RoutingTable is now defined in utils/dht_core_utils.hpp
class TransactionManager;
class MessageSender;
struct PeerLookupState;

/**
 * @brief Manages query operations for peer lookups
 */
class PeerLookupQueryManager {
public:
    /**
     * @brief Constructor
     * @param config The DHT configuration
     * @param nodeID The node ID
     * @param routingTable The routing table
     * @param transactionManager The transaction manager
     * @param messageSender The message sender
     */
    PeerLookupQueryManager(const DHTConfig& config,
                          const NodeID& nodeID,
                          std::shared_ptr<dht::RoutingTable> routingTable,
                          std::shared_ptr<TransactionManager> transactionManager,
                          std::shared_ptr<MessageSender> messageSender);

    /**
     * @brief Sends get_peers queries to the closest nodes
     * @param lookupID The lookup ID
     * @param lookup The lookup state
     * @param responseCallback Callback for handling responses
     * @param errorCallback Callback for handling errors
     * @param timeoutCallback Callback for handling timeouts
     * @return True if queries were sent, false otherwise
     */
    bool sendQueries(
        const std::string& lookupID,
        PeerLookupState& lookup,
        std::function<void(std::shared_ptr<ResponseMessage>, const network::EndPoint&)> responseCallback,
        std::function<void(std::shared_ptr<ErrorMessage>, const network::EndPoint&)> errorCallback,
        std::function<void(const NodeID&)> timeoutCallback);

    /**
     * @brief Checks if a lookup is complete
     * @param lookup The lookup state
     * @param kBucketSize The k-bucket size
     * @return True if the lookup is complete, false otherwise
     */
    bool isLookupComplete(const PeerLookupState& lookup, size_t kBucketSize) const;

private:
    DHTConfig m_config;
    NodeID m_nodeID;
    std::shared_ptr<dht::RoutingTable> m_routingTable;
    std::shared_ptr<TransactionManager> m_transactionManager;
    std::shared_ptr<MessageSender> m_messageSender;
};

} // namespace dht_hunter::dht
