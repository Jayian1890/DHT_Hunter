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
class PeerStorage;
struct PeerLookupState;

/**
 * @brief Handles responses for peer lookups
 */
class PeerLookupResponseHandler {
public:
    /**
     * @brief Constructor
     * @param config The DHT configuration
     * @param nodeID The node ID
     * @param routingTable The routing table
     * @param peerStorage The peer storage
     */
    PeerLookupResponseHandler(const DHTConfig& config,
                             const NodeID& nodeID,
                             std::shared_ptr<RoutingTable> routingTable,
                             std::shared_ptr<PeerStorage> peerStorage);
    
    /**
     * @brief Handles a get_peers response
     * @param lookupID The lookup ID
     * @param lookup The lookup state
     * @param response The response
     * @param sender The sender
     * @return True if more queries should be sent, false otherwise
     */
    bool handleResponse(
        const std::string& lookupID, 
        PeerLookupState& lookup,
        std::shared_ptr<GetPeersResponse> response, 
        const network::EndPoint& sender);
    
    /**
     * @brief Handles an error
     * @param lookup The lookup state
     * @param error The error
     * @param sender The sender
     * @return True if more queries should be sent, false otherwise
     */
    bool handleError(
        PeerLookupState& lookup,
        std::shared_ptr<ErrorMessage> error, 
        const network::EndPoint& sender);
    
    /**
     * @brief Handles a timeout
     * @param lookup The lookup state
     * @param nodeID The node ID
     * @return True if more queries should be sent, false otherwise
     */
    bool handleTimeout(
        PeerLookupState& lookup, 
        const NodeID& nodeID);
    
    /**
     * @brief Handles an announce_peer response
     * @param lookup The lookup state
     * @param response The response
     * @param sender The sender
     * @return True if the announcement is complete, false otherwise
     */
    bool handleAnnounceResponse(
        PeerLookupState& lookup,
        std::shared_ptr<AnnouncePeerResponse> response, 
        const network::EndPoint& sender);
    
    /**
     * @brief Handles an announce_peer error
     * @param lookup The lookup state
     * @param error The error
     * @param sender The sender
     * @return True if the announcement is complete, false otherwise
     */
    bool handleAnnounceError(
        PeerLookupState& lookup,
        std::shared_ptr<ErrorMessage> error, 
        const network::EndPoint& sender);
    
    /**
     * @brief Handles an announce_peer timeout
     * @param lookup The lookup state
     * @param nodeID The node ID
     * @return True if the announcement is complete, false otherwise
     */
    bool handleAnnounceTimeout(
        PeerLookupState& lookup,
        const NodeID& nodeID);
    
    /**
     * @brief Completes a lookup
     * @param lookup The lookup state
     */
    void completeLookup(PeerLookupState& lookup) const;
    
private:
    DHTConfig m_config;
    NodeID m_nodeID;
    std::shared_ptr<RoutingTable> m_routingTable;
    std::shared_ptr<PeerStorage> m_peerStorage;
};

} // namespace dht_hunter::dht
