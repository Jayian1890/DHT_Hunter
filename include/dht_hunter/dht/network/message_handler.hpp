#pragma once

#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/dht/types/dht_types.hpp"
#include "dht_hunter/dht/network/message.hpp"
#include "dht_hunter/dht/network/query_message.hpp"
#include "dht_hunter/dht/network/response_message.hpp"
#include "dht_hunter/dht/network/error_message.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"
#include <memory>
#include <mutex>
#include <functional>

namespace dht_hunter::dht {

// Forward declarations
class RoutingTable;
class TokenManager;
class PeerStorage;
class TransactionManager;

/**
 * @brief Handles DHT messages (Singleton)
 */
class MessageHandler {
public:
    /**
     * @brief Gets the singleton instance of the message handler
     * @param config The DHT configuration (only used if instance doesn't exist yet)
     * @param nodeID The node ID (only used if instance doesn't exist yet)
     * @param messageSender The message sender (only used if instance doesn't exist yet)
     * @param routingTable The routing table (only used if instance doesn't exist yet)
     * @param tokenManager The token manager (only used if instance doesn't exist yet)
     * @param peerStorage The peer storage (only used if instance doesn't exist yet)
     * @param transactionManager The transaction manager (only used if instance doesn't exist yet)
     * @return The singleton instance
     */
    static std::shared_ptr<MessageHandler> getInstance(
        const DHTConfig& config,
        const NodeID& nodeID,
        std::shared_ptr<MessageSender> messageSender,
        std::shared_ptr<RoutingTable> routingTable,
        std::shared_ptr<TokenManager> tokenManager,
        std::shared_ptr<PeerStorage> peerStorage,
        std::shared_ptr<TransactionManager> transactionManager);

    /**
     * @brief Destructor
     */
    ~MessageHandler();

    /**
     * @brief Delete copy constructor and assignment operator
     */
    MessageHandler(const MessageHandler&) = delete;
    MessageHandler& operator=(const MessageHandler&) = delete;
    MessageHandler(MessageHandler&&) = delete;
    MessageHandler& operator=(MessageHandler&&) = delete;

    /**
     * @brief Handles a raw message
     * @param data The message data
     * @param size The message size
     * @param sender The sender
     */
    void handleRawMessage(const uint8_t* data, size_t size, const network::EndPoint& sender);

    /**
     * @brief Handles a message
     * @param message The message
     * @param sender The sender
     */
    void handleMessage(std::shared_ptr<Message> message, const network::EndPoint& sender);

private:
    /**
     * @brief Handles a query message
     * @param query The query message
     * @param sender The sender
     */
    void handleQuery(std::shared_ptr<QueryMessage> query, const network::EndPoint& sender);

    /**
     * @brief Handles a response message
     * @param response The response message
     * @param sender The sender
     */
    void handleResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender);

    /**
     * @brief Handles an error message
     * @param error The error message
     * @param sender The sender
     */
    void handleError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender);

    /**
     * @brief Handles a ping query
     * @param query The ping query
     * @param sender The sender
     */
    void handlePingQuery(std::shared_ptr<PingQuery> query, const network::EndPoint& sender);

    /**
     * @brief Handles a find_node query
     * @param query The find_node query
     * @param sender The sender
     */
    void handleFindNodeQuery(std::shared_ptr<FindNodeQuery> query, const network::EndPoint& sender);

    /**
     * @brief Handles a get_peers query
     * @param query The get_peers query
     * @param sender The sender
     */
    void handleGetPeersQuery(std::shared_ptr<GetPeersQuery> query, const network::EndPoint& sender);

    /**
     * @brief Handles an announce_peer query
     * @param query The announce_peer query
     * @param sender The sender
     */
    void handleAnnouncePeerQuery(std::shared_ptr<AnnouncePeerQuery> query, const network::EndPoint& sender);

    /**
     * @brief Updates the routing table with a node
     * @param nodeID The node ID
     * @param endpoint The endpoint
     */
    void updateRoutingTable(const NodeID& nodeID, const network::EndPoint& endpoint);

    /**
     * @brief Private constructor for singleton pattern
     */
    MessageHandler(const DHTConfig& config,
                  const NodeID& nodeID,
                  std::shared_ptr<MessageSender> messageSender,
                  std::shared_ptr<RoutingTable> routingTable,
                  std::shared_ptr<TokenManager> tokenManager,
                  std::shared_ptr<PeerStorage> peerStorage,
                  std::shared_ptr<TransactionManager> transactionManager);

    // Static instance for singleton pattern
    static std::shared_ptr<MessageHandler> s_instance;
    static std::mutex s_instanceMutex;

    DHTConfig m_config;
    NodeID m_nodeID;
    std::shared_ptr<MessageSender> m_messageSender;
    std::shared_ptr<RoutingTable> m_routingTable;
    std::shared_ptr<TokenManager> m_tokenManager;
    std::shared_ptr<PeerStorage> m_peerStorage;
    std::shared_ptr<TransactionManager> m_transactionManager;
    std::shared_ptr<unified_event::EventBus> m_eventBus;
    std::mutex m_mutex;
    event::Logger m_logger;
};

} // namespace dht_hunter::dht
