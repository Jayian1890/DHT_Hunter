#pragma once

#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/dht/types.hpp"
#include "dht_hunter/dht/network/response_message.hpp"
#include "dht_hunter/dht/network/error_message.hpp"
#include <memory>
#include <string>
#include <vector>

namespace dht_hunter::dht {

// Forward declarations
class RoutingTable;
struct NodeLookupState;

/**
 * @brief Handles responses for node lookups
 */
class NodeLookupResponseHandler {
public:
    /**
     * @brief Constructor
     * @param config The DHT configuration
     * @param nodeID The node ID
     * @param routingTable The routing table
     */
    NodeLookupResponseHandler(const DHTConfig& config,
                             const NodeID& nodeID,
                             std::shared_ptr<RoutingTable> routingTable);
    
    /**
     * @brief Handles a find_node response
     * @param lookupID The lookup ID
     * @param lookup The lookup state
     * @param response The response
     * @param sender The sender
     * @return True if more queries should be sent, false otherwise
     */
    bool handleResponse(
        const std::string& lookupID, 
        NodeLookupState& lookup,
        std::shared_ptr<FindNodeResponse> response, 
        const network::EndPoint& sender);
    
    /**
     * @brief Handles an error
     * @param lookup The lookup state
     * @param error The error
     * @param sender The sender
     * @return True if more queries should be sent, false otherwise
     */
    bool handleError(
        NodeLookupState& lookup,
        std::shared_ptr<ErrorMessage> error, 
        const network::EndPoint& sender);
    
    /**
     * @brief Handles a timeout
     * @param lookup The lookup state
     * @param nodeID The node ID
     * @return True if more queries should be sent, false otherwise
     */
    bool handleTimeout(
        NodeLookupState& lookup, 
        const NodeID& nodeID);
    
    /**
     * @brief Completes a lookup
     * @param lookup The lookup state
     */
    void completeLookup(NodeLookupState& lookup) const;
    
private:
    DHTConfig m_config;
    NodeID m_nodeID;
    std::shared_ptr<RoutingTable> m_routingTable;
};

} // namespace dht_hunter::dht
