#pragma once

#include "dht_hunter/dht/core/dht_node_config.hpp"
#include "dht_hunter/dht/core/dht_types.hpp"
#include "dht_hunter/dht/network/dht_message.hpp"
#include "dht_hunter/dht/network/dht_response_message.hpp"
#include "dht_hunter/dht/network/dht_error_message.hpp"
#include "dht_hunter/dht/transactions/dht_transaction_types.hpp"
#include "dht_hunter/util/mutex_utils.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <atomic>

namespace dht_hunter::dht {

// Forward declarations
class DHTRoutingManager;
class DHTTransactionManager;
class DHTPeerStorage;
class DHTTokenManager;

/**
 * @brief Callback for get_peers lookup completion
 * @param peers The peers found
 * @param nodes The closest nodes found (if no peers were found)
 * @param token The token to use for announce_peer (if nodes were returned)
 */
using GetPeersLookupCallback = std::function<void(
    const std::vector<network::EndPoint>&,
    const std::vector<std::shared_ptr<Node>>&,
    const std::string&)>;

/**
 * @brief Represents a peer lookup operation
 */
class PeerLookup {
public:
    /**
     * @brief Constructs a peer lookup
     * @param id The lookup ID
     * @param infoHash The info hash to look up
     * @param callback The callback to call when the lookup is complete
     */
    PeerLookup(const std::string& id, const InfoHash& infoHash, GetPeersLookupCallback callback);

    /**
     * @brief Gets the lookup ID
     * @return The lookup ID
     */
    const std::string& getID() const;

    /**
     * @brief Gets the info hash
     * @return The info hash
     */
    const InfoHash& getInfoHash() const;

    /**
     * @brief Gets the closest nodes found so far
     * @return The closest nodes
     */
    std::vector<std::shared_ptr<Node>> getClosestNodes() const;

    /**
     * @brief Gets the peers found so far
     * @return The peers
     */
    std::vector<network::EndPoint> getPeers() const;

    /**
     * @brief Gets the tokens received from nodes
     * @return The tokens
     */
    std::unordered_map<NodeID, std::string> getTokens() const;

    /**
     * @brief Adds a node to the lookup
     * @param node The node to add
     * @return True if the node was added, false otherwise
     */
    bool addNode(std::shared_ptr<Node> node);

    /**
     * @brief Adds a peer to the lookup
     * @param peer The peer to add
     * @return True if the peer was added, false otherwise
     */
    bool addPeer(const network::EndPoint& peer);

    /**
     * @brief Adds a token for a node
     * @param nodeID The node ID
     * @param token The token
     */
    void addToken(const NodeID& nodeID, const std::string& token);

    /**
     * @brief Marks a node as queried
     * @param nodeID The node ID
     */
    void markNodeAsQueried(const NodeID& nodeID);

    /**
     * @brief Marks a node as responded
     * @param nodeID The node ID
     */
    void markNodeAsResponded(const NodeID& nodeID);

    /**
     * @brief Marks a node as failed
     * @param nodeID The node ID
     */
    void markNodeAsFailed(const NodeID& nodeID);

    /**
     * @brief Gets the next nodes to query
     * @param count The maximum number of nodes to return
     * @return The next nodes to query
     */
    std::vector<std::shared_ptr<Node>> getNextNodesToQuery(size_t count);

    /**
     * @brief Checks if the lookup is complete
     * @return True if the lookup is complete, false otherwise
     */
    bool isComplete() const;

    /**
     * @brief Completes the lookup
     */
    void complete();

private:
    std::string m_id;
    InfoHash m_infoHash;
    GetPeersLookupCallback m_callback;
    std::vector<std::shared_ptr<Node>> m_closestNodes;
    std::vector<network::EndPoint> m_peers;
    std::unordered_map<NodeID, std::string> m_tokens;
    std::unordered_set<NodeID> m_queriedNodes;
    std::unordered_set<NodeID> m_respondedNodes;
    std::unordered_set<NodeID> m_failedNodes;
    mutable util::CheckedMutex m_mutex;
    std::atomic<bool> m_complete{false};
};

/**
 * @brief Manages peer lookups
 */
class DHTPeerLookup {
public:
    /**
     * @brief Constructs a peer lookup manager
     * @param config The DHT node configuration
     * @param nodeID The node ID
     * @param routingManager The routing manager to use for finding nodes
     * @param transactionManager The transaction manager to use for sending queries
     * @param peerStorage The peer storage to use for storing peers
     * @param tokenManager The token manager to use for validating tokens
     */
    DHTPeerLookup(const DHTNodeConfig& config,
                 const NodeID& nodeID,
                 std::shared_ptr<DHTRoutingManager> routingManager,
                 std::shared_ptr<DHTTransactionManager> transactionManager,
                 std::shared_ptr<DHTPeerStorage> peerStorage,
                 std::shared_ptr<DHTTokenManager> tokenManager);

    /**
     * @brief Destructor
     */
    ~DHTPeerLookup() = default;

    /**
     * @brief Starts a peer lookup
     * @param infoHash The info hash to look up
     * @param callback The callback to call when the lookup is complete
     * @return The lookup ID, or an empty string if the lookup could not be started
     */
    std::string startLookup(const InfoHash& infoHash, GetPeersLookupCallback callback);

    /**
     * @brief Announces as a peer for an info hash
     * @param infoHash The info hash
     * @param port The port to announce
     * @param callback The callback to call when the announcement is complete
     * @return True if the announcement was started, false otherwise
     */
    bool announceAsPeer(const InfoHash& infoHash, uint16_t port, std::function<void(bool)> callback);

    /**
     * @brief Handles a get_peers response
     * @param response The response message
     * @param sender The sender's endpoint
     * @return True if the response was handled, false otherwise
     */
    bool handleGetPeersResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender);

    /**
     * @brief Handles a get_peers error
     * @param error The error message
     * @param sender The sender's endpoint
     * @return True if the error was handled, false otherwise
     */
    bool handleGetPeersError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender);

    /**
     * @brief Handles a get_peers timeout
     * @param transactionID The transaction ID
     * @return True if the timeout was handled, false otherwise
     */
    bool handleGetPeersTimeout(const std::string& transactionID);

    /**
     * @brief Handles an announce_peer response
     * @param response The response message
     * @param sender The sender's endpoint
     * @return True if the response was handled, false otherwise
     */
    bool handleAnnouncePeerResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender);

    /**
     * @brief Handles an announce_peer error
     * @param error The error message
     * @param sender The sender's endpoint
     * @return True if the error was handled, false otherwise
     */
    bool handleAnnouncePeerError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender);

    /**
     * @brief Handles an announce_peer timeout
     * @param transactionID The transaction ID
     * @return True if the timeout was handled, false otherwise
     */
    bool handleAnnouncePeerTimeout(const std::string& transactionID);

private:
    /**
     * @brief Continues a peer lookup
     * @param lookup The lookup to continue
     */
    void continueLookup(std::shared_ptr<PeerLookup> lookup);

    /**
     * @brief Completes an announcement
     * @param infoHash The info hash
     * @param success Whether the announcement was successful
     * @param callback The callback to call
     */
    void completeAnnouncement(const InfoHash& infoHash, bool success, std::function<void(bool)> callback);

    /**
     * @brief Generates a random lookup ID
     * @return The lookup ID
     */
    std::string generateLookupID() const;

    /**
     * @brief Finds a lookup by transaction ID
     * @param transactionID The transaction ID
     * @return The lookup, or nullptr if not found
     */
    std::shared_ptr<PeerLookup> findLookupByTransactionID(const std::string& transactionID);

    /**
     * @brief Finds an announcement by transaction ID
     * @param transactionID The transaction ID
     * @return The announcement info, or nullptr if not found
     */
    std::shared_ptr<std::tuple<InfoHash, uint16_t, std::function<void(bool)>>> findAnnouncementByTransactionID(const std::string& transactionID);

    DHTNodeConfig m_config;
    NodeID m_nodeID;
    std::shared_ptr<DHTRoutingManager> m_routingManager;
    std::shared_ptr<DHTTransactionManager> m_transactionManager;
    std::shared_ptr<DHTPeerStorage> m_peerStorage;
    std::shared_ptr<DHTTokenManager> m_tokenManager;
    std::unordered_map<std::string, std::shared_ptr<PeerLookup>> m_lookups;
    std::unordered_map<std::string, std::string> m_transactionToLookup;
    std::unordered_map<std::string, std::shared_ptr<std::tuple<InfoHash, uint16_t, std::function<void(bool)>>>> m_announcements;
    std::unordered_map<std::string, std::string> m_transactionToAnnouncement;
    util::CheckedMutex m_lookupsMutex;
    util::CheckedMutex m_announcementsMutex;
};

} // namespace dht_hunter::dht
