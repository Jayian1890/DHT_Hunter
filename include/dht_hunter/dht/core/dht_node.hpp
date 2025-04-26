#pragma once

#include "dht_hunter/dht/core/dht_types.hpp"
#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/dht/core/routing_table.hpp"
#include "dht_hunter/event/logger.hpp"
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

namespace dht_hunter::dht {

// Forward declarations
class SocketManager;
class MessageSender;
class MessageHandler;
class TokenManager;
class PeerStorage;
class TransactionManager;
class RoutingManager;
class NodeLookup;
class PeerLookup;
class Bootstrapper;

/**
 * @brief A DHT node
 */
class DHTNode {
public:
    /**
     * @brief Constructs a DHT node
     * @param config The DHT configuration
     */
    explicit DHTNode(const DHTConfig& config = DHTConfig());

    /**
     * @brief Destructor
     */
    ~DHTNode();

    /**
     * @brief Starts the DHT node
     * @return True if the node was started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stops the DHT node
     */
    void stop();

    /**
     * @brief Checks if the DHT node is running
     * @return True if the node is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Gets the node ID
     * @return The node ID
     */
    const NodeID& getNodeID() const;

    /**
     * @brief Gets the port
     * @return The port
     */
    uint16_t getPort() const;

    /**
     * @brief Gets the routing table
     * @return The routing table
     */
    std::shared_ptr<RoutingTable> getRoutingTable() const;

    /**
     * @brief Finds the closest nodes to a target ID
     * @param targetID The target ID
     * @param callback The callback to call with the result
     */
    void findClosestNodes(const NodeID& targetID, std::function<void(const std::vector<std::shared_ptr<Node>>&)> callback);

    /**
     * @brief Gets peers for an info hash
     * @param infoHash The info hash
     * @param callback The callback to call with the result
     */
    void getPeers(const InfoHash& infoHash, std::function<void(const std::vector<network::EndPoint>&)> callback);

    /**
     * @brief Announces a peer for an info hash
     * @param infoHash The info hash
     * @param port The port
     * @param callback The callback to call with the result
     */
    void announcePeer(const InfoHash& infoHash, uint16_t port, std::function<void(bool)> callback);

    /**
     * @brief Pings a node
     * @param node The node to ping
     * @param callback The callback to call with the result
     */
    void ping(const std::shared_ptr<Node>& node, std::function<void(bool)> callback);

private:
    /**
     * @brief Saves the routing table periodically
     */
    void saveRoutingTablePeriodically();

    NodeID m_nodeID;
    DHTConfig m_config;
    std::atomic<bool> m_running;
    std::shared_ptr<RoutingTable> m_routingTable;
    std::shared_ptr<SocketManager> m_socketManager;
    std::shared_ptr<MessageSender> m_messageSender;
    std::shared_ptr<MessageHandler> m_messageHandler;
    std::shared_ptr<TokenManager> m_tokenManager;
    std::shared_ptr<PeerStorage> m_peerStorage;
    std::shared_ptr<TransactionManager> m_transactionManager;
    std::shared_ptr<RoutingManager> m_routingManager;
    std::shared_ptr<NodeLookup> m_nodeLookup;
    std::shared_ptr<PeerLookup> m_peerLookup;
    std::shared_ptr<Bootstrapper> m_bootstrapper;
    std::thread m_saveRoutingTableThread;
    std::mutex m_mutex;
    event::Logger m_logger;
};

} // namespace dht_hunter::dht
