#pragma once

#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/dht/types/dht_types.hpp"
#include "dht_hunter/dht/network/message.hpp"
#include "dht_hunter/dht/network/response_message.hpp"
#include "dht_hunter/dht/network/error_message.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <mutex>

namespace dht_hunter::dht {

// Forward declarations
class RoutingTable;
class TransactionManager;
class MessageSender;
class TokenManager;
class PeerStorage;

/**
 * @brief Performs a peer lookup (Singleton)
 */
class PeerLookup {
public:
    /**
     * @brief Gets the singleton instance of the peer lookup
     * @param config The DHT configuration (only used if instance doesn't exist yet)
     * @param nodeID The node ID (only used if instance doesn't exist yet)
     * @param routingTable The routing table (only used if instance doesn't exist yet)
     * @param transactionManager The transaction manager (only used if instance doesn't exist yet)
     * @param messageSender The message sender (only used if instance doesn't exist yet)
     * @param tokenManager The token manager (only used if instance doesn't exist yet)
     * @param peerStorage The peer storage (only used if instance doesn't exist yet)
     * @return The singleton instance
     */
    static std::shared_ptr<PeerLookup> getInstance(
        const DHTConfig& config,
        const NodeID& nodeID,
        std::shared_ptr<RoutingTable> routingTable,
        std::shared_ptr<TransactionManager> transactionManager,
        std::shared_ptr<MessageSender> messageSender,
        std::shared_ptr<TokenManager> tokenManager,
        std::shared_ptr<PeerStorage> peerStorage);

    /**
     * @brief Destructor
     */
    ~PeerLookup();

    /**
     * @brief Delete copy constructor and assignment operator
     */
    PeerLookup(const PeerLookup&) = delete;
    PeerLookup& operator=(const PeerLookup&) = delete;
    PeerLookup(PeerLookup&&) = delete;
    PeerLookup& operator=(PeerLookup&&) = delete;

    /**
     * @brief Performs a peer lookup
     * @param infoHash The info hash
     * @param callback The callback to call with the result
     */
    void lookup(const InfoHash& infoHash, std::function<void(const std::vector<network::EndPoint>&)> callback);

    /**
     * @brief Announces a peer
     * @param infoHash The info hash
     * @param port The port
     * @param callback The callback to call with the result
     */
    void announce(const InfoHash& infoHash, uint16_t port, std::function<void(bool)> callback);

private:
    /**
     * @brief Sends get_peers queries to the closest nodes
     * @param lookupID The lookup ID
     */
    void sendQueries(const std::string& lookupID);

    /**
     * @brief Handles a get_peers response
     * @param lookupID The lookup ID
     * @param response The response
     * @param sender The sender
     */
    void handleResponse(const std::string& lookupID, std::shared_ptr<GetPeersResponse> response, const network::EndPoint& sender);

    /**
     * @brief Handles an error
     * @param lookupID The lookup ID
     * @param error The error
     * @param sender The sender
     */
    void handleError(const std::string& lookupID, std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender);

    /**
     * @brief Handles a timeout
     * @param lookupID The lookup ID
     * @param nodeID The node ID
     */
    void handleTimeout(const std::string& lookupID, const NodeID& nodeID);

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
     * @brief Announces a peer to nodes with tokens
     * @param lookupID The lookup ID
     */
    void announceToNodes(const std::string& lookupID);

    /**
     * @brief Handles an announce_peer response
     * @param lookupID The lookup ID
     * @param response The response
     * @param sender The sender
     */
    void handleAnnounceResponse(const std::string& lookupID, std::shared_ptr<AnnouncePeerResponse> response, const network::EndPoint& sender);

    /**
     * @brief Handles an announce_peer error
     * @param lookupID The lookup ID
     * @param error The error
     * @param sender The sender
     */
    void handleAnnounceError(const std::string& lookupID, std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender);

    /**
     * @brief Handles an announce_peer timeout
     * @param lookupID The lookup ID
     * @param nodeID The node ID
     */
    void handleAnnounceTimeout(const std::string& lookupID, const NodeID& nodeID);

    /**
     * @brief Completes an announcement
     * @param lookupID The lookup ID
     * @param success Whether the announcement was successful
     */
    void completeAnnouncement(const std::string& lookupID, bool success);

    /**
     * @brief A lookup state
     */
    struct Lookup {
        InfoHash infoHash;
        std::vector<std::shared_ptr<Node>> nodes;
        std::vector<network::EndPoint> peers;
        std::unordered_set<std::string> queriedNodes;
        std::unordered_set<std::string> respondedNodes;
        std::unordered_set<std::string> activeQueries;
        std::unordered_map<std::string, std::string> nodeTokens;
        std::unordered_set<std::string> announcedNodes;
        std::unordered_set<std::string> activeAnnouncements;
        size_t iteration;
        uint16_t port;
        std::function<void(const std::vector<network::EndPoint>&)> lookupCallback;
        std::function<void(bool)> announceCallback;
        bool announcing;

        Lookup(const InfoHash& infoHash, std::function<void(const std::vector<network::EndPoint>&)> callback)
            : infoHash(infoHash), iteration(0), lookupCallback(callback), announcing(false) {}

        Lookup(const InfoHash& infoHash, uint16_t port, std::function<void(bool)> callback)
            : infoHash(infoHash), iteration(0), port(port), announceCallback(callback), announcing(true) {}
    };

    /**
     * @brief Private constructor for singleton pattern
     */
    PeerLookup(const DHTConfig& config,
              const NodeID& nodeID,
              std::shared_ptr<RoutingTable> routingTable,
              std::shared_ptr<TransactionManager> transactionManager,
              std::shared_ptr<MessageSender> messageSender,
              std::shared_ptr<TokenManager> tokenManager,
              std::shared_ptr<PeerStorage> peerStorage);

    // Static instance for singleton pattern
    static std::shared_ptr<PeerLookup> s_instance;
    static std::mutex s_instanceMutex;

    DHTConfig m_config;
    NodeID m_nodeID;
    std::shared_ptr<RoutingTable> m_routingTable;
    std::shared_ptr<TransactionManager> m_transactionManager;
    std::shared_ptr<MessageSender> m_messageSender;
    std::shared_ptr<TokenManager> m_tokenManager;
    std::shared_ptr<PeerStorage> m_peerStorage;
    std::unordered_map<std::string, Lookup> m_lookups;
    std::mutex m_mutex;
    event::Logger m_logger;
};

} // namespace dht_hunter::dht
