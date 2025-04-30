#pragma once

#include "dht_hunter/dht/node_lookup/components/base_node_lookup_component.hpp"
#include "dht_hunter/dht/node_lookup/components/node_lookup_state.hpp"
#include "dht_hunter/dht/transactions/transaction_manager.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include <unordered_map>
#include <string>
#include <functional>

namespace dht_hunter::dht::node_lookup {

/**
 * @brief Manager for node lookups
 * 
 * This component manages node lookups, including creating, tracking, and completing lookups.
 */
class NodeLookupManager : public BaseNodeLookupComponent {
public:
    /**
     * @brief Constructs a node lookup manager
     * @param config The DHT configuration
     * @param nodeID The node ID
     * @param routingTable The routing table
     * @param transactionManager The transaction manager
     * @param messageSender The message sender
     */
    NodeLookupManager(const DHTConfig& config,
                     const NodeID& nodeID,
                     std::shared_ptr<RoutingTable> routingTable,
                     std::shared_ptr<TransactionManager> transactionManager,
                     std::shared_ptr<MessageSender> messageSender);

    /**
     * @brief Performs a node lookup
     * @param targetID The target ID
     * @param callback The callback to call with the result
     */
    void lookup(const NodeID& targetID, std::function<void(const std::vector<std::shared_ptr<Node>>&)> callback);

    /**
     * @brief Handles a response
     * @param lookupID The lookup ID
     * @param response The response
     * @param sender The sender
     */
    void handleResponse(const std::string& lookupID, std::shared_ptr<FindNodeResponse> response, const EndPoint& sender);

    /**
     * @brief Handles an error
     * @param lookupID The lookup ID
     * @param error The error
     * @param sender The sender
     */
    void handleError(const std::string& lookupID, std::shared_ptr<ErrorMessage> error, const EndPoint& sender);

    /**
     * @brief Handles a timeout
     * @param lookupID The lookup ID
     * @param nodeID The node ID
     */
    void handleTimeout(const std::string& lookupID, const NodeID& nodeID);

protected:
    /**
     * @brief Initializes the component
     * @return True if the component was initialized successfully, false otherwise
     */
    bool onInitialize() override;

    /**
     * @brief Starts the component
     * @return True if the component was started successfully, false otherwise
     */
    bool onStart() override;

    /**
     * @brief Stops the component
     */
    void onStop() override;

private:
    /**
     * @brief Sends queries for a lookup
     * @param lookupID The lookup ID
     */
    void sendQueries(const std::string& lookupID);

    /**
     * @brief Checks if a lookup is complete
     * @param lookupID The lookup ID
     * @return True if the lookup is complete, false otherwise
     */
    bool isLookupComplete(const std::string& lookupID);

    /**
     * @brief Completes a lookup
     * @param lookupID The lookup ID
     */
    void completeLookup(const std::string& lookupID);

    /**
     * @brief Generates a lookup ID
     * @param targetID The target ID
     * @return The lookup ID
     */
    std::string generateLookupID(const NodeID& targetID) const;

    std::shared_ptr<TransactionManager> m_transactionManager;
    std::shared_ptr<MessageSender> m_messageSender;
    std::unordered_map<std::string, NodeLookupState> m_lookups;
};

} // namespace dht_hunter::dht::node_lookup
