#pragma once

#include "dht_hunter/dht/core/dht_node_config.hpp"
#include "dht_hunter/dht/network/dht_message_sender.hpp"
#include "dht_hunter/dht/core/dht_types.hpp"
#include "dht_hunter/dht/network/dht_message.hpp"
#include "dht_hunter/dht/network/dht_query_message.hpp"
#include "dht_hunter/dht/network/dht_response_message.hpp"
#include "dht_hunter/dht/network/dht_error_message.hpp"
#include "dht_hunter/dht/transactions/dht_transaction_types.hpp"
#include "dht_hunter/util/mutex_utils.hpp"
#include <memory>
#include <functional>

// Forward declarations
namespace dht_hunter {
    namespace dht {
        class DHTRoutingManager;
        class DHTPeerStorage;
        class DHTTokenManager;
    }

    namespace crawler {
        class InfoHashCollector;
    }
}

#include "dht_hunter/dht/transactions/dht_transaction_manager.hpp"

namespace dht_hunter::dht {

/**
 * @brief Handles incoming DHT messages
 */
class DHTMessageHandler {
public:
    /**
     * @brief Constructs a message handler
     * @param config The DHT node configuration
     * @param nodeID The node ID
     * @param messageSender The message sender to use for sending responses
     * @param routingManager The routing manager to use for updating the routing table
     * @param transactionManager The transaction manager to use for finding transactions
     * @param peerStorage The peer storage to use for storing and retrieving peers
     * @param tokenManager The token manager to use for generating and validating tokens
     */
    DHTMessageHandler(const DHTNodeConfig& config,
                     const NodeID& nodeID,
                     std::shared_ptr<DHTMessageSender> messageSender,
                     std::shared_ptr<DHTRoutingManager> routingManager,
                     std::shared_ptr<DHTTransactionManager> transactionManager,
                     std::shared_ptr<DHTPeerStorage> peerStorage,
                     std::shared_ptr<DHTTokenManager> tokenManager);

    /**
     * @brief Destructor
     */
    ~DHTMessageHandler() = default;

    /**
     * @brief Sets the InfoHash collector
     * @param collector The InfoHash collector
     */
    void setInfoHashCollector(std::shared_ptr<crawler::InfoHashCollector> collector);

    /**
     * @brief Gets the InfoHash collector
     * @return The InfoHash collector
     */
    std::shared_ptr<crawler::InfoHashCollector> getInfoHashCollector() const;

    /**
     * @brief Handles a raw message
     * @param data The message data
     * @param size The message size
     * @param sender The sender's endpoint
     */
    void handleRawMessage(const uint8_t* data, size_t size, const network::EndPoint& sender);

    /**
     * @brief Handles a decoded message
     * @param message The message
     * @param sender The sender's endpoint
     */
    void handleMessage(std::shared_ptr<DHTMessage> message, const network::EndPoint& sender);

    /**
     * @brief Handles a query message
     * @param query The query message
     * @param sender The sender's endpoint
     */
    void handleQuery(std::shared_ptr<QueryMessage> query, const network::EndPoint& sender);

    /**
     * @brief Handles a response message
     * @param response The response message
     * @param sender The sender's endpoint
     */
    void handleResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender);

    /**
     * @brief Handles an error message
     * @param error The error message
     * @param sender The sender's endpoint
     */
    void handleError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender);

private:
    /**
     * @brief Handles a ping query
     * @param query The ping query
     * @param sender The sender's endpoint
     */
    void handlePingQuery(std::shared_ptr<PingQuery> query, const network::EndPoint& sender);

    /**
     * @brief Handles a find_node query
     * @param query The find_node query
     * @param sender The sender's endpoint
     */
    void handleFindNodeQuery(std::shared_ptr<FindNodeQuery> query, const network::EndPoint& sender);

    /**
     * @brief Handles a get_peers query
     * @param query The get_peers query
     * @param sender The sender's endpoint
     */
    void handleGetPeersQuery(std::shared_ptr<GetPeersQuery> query, const network::EndPoint& sender);

    /**
     * @brief Handles an announce_peer query
     * @param query The announce_peer query
     * @param sender The sender's endpoint
     */
    void handleAnnouncePeerQuery(std::shared_ptr<AnnouncePeerQuery> query, const network::EndPoint& sender);

    /**
     * @brief Updates the routing table with a node
     * @param nodeID The node ID
     * @param endpoint The node's endpoint
     */
    void updateRoutingTable(const NodeID& nodeID, const network::EndPoint& endpoint);

    DHTNodeConfig m_config;
    NodeID m_nodeID;
    std::shared_ptr<DHTMessageSender> m_messageSender;
    std::shared_ptr<DHTRoutingManager> m_routingManager;
    std::shared_ptr<DHTTransactionManager> m_transactionManager;
    std::shared_ptr<DHTPeerStorage> m_peerStorage;
    std::shared_ptr<DHTTokenManager> m_tokenManager;
    std::shared_ptr<crawler::InfoHashCollector> m_infoHashCollector;
    util::CheckedMutex m_infoHashCollectorMutex;
};

} // namespace dht_hunter::dht
