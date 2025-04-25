#include "dht_hunter/dht/core/dht_node.hpp"
#include "dht_hunter/dht/storage/dht_token_manager.hpp"
#include "dht_hunter/dht/storage/dht_peer_storage.hpp"
#include "dht_hunter/dht/network/dht_socket_manager.hpp"
#include "dht_hunter/dht/network/dht_message_sender.hpp"
#include "dht_hunter/dht/network/dht_message_handler.hpp"
#include "dht_hunter/dht/routing/dht_routing_manager.hpp"
#include "dht_hunter/dht/routing/dht_node_lookup.hpp"
#include "dht_hunter/dht/routing/dht_peer_lookup.hpp"
#include "dht_hunter/dht/transactions/dht_transaction_manager.hpp"
#include "dht_hunter/dht/persistence/dht_persistence_manager.hpp"
#include "dht_hunter/dht/routing_table.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include <filesystem>

DEFINE_COMPONENT_LOGGER("DHT", "Node")

namespace dht_hunter::dht {

DHTNode::DHTNode(const DHTNodeConfig& config, const NodeID& nodeID)
    : m_nodeID(nodeID),
      m_port(config.getPort()),
      m_config(config),
      m_running(false),
      m_routingTable(nodeID, config.getKBucketSize()),
      m_routingTablePath(config.getRoutingTablePath()),
      m_peerCachePath(config.getPeerCachePath()),
      m_transactionsPath(config.getTransactionsPath().empty() ?
                        config.getConfigDir() + "/transactions.dat" :
                        config.getTransactionsPath()) {

    getLogger()->info("Creating DHT node with ID: {}, port: {}, config path: {}",
                 nodeIDToString(m_nodeID), config.getPort(), m_routingTablePath);

    // Initialize components
    m_tokenManager = std::make_shared<DHTTokenManager>();
    m_socketManager = std::make_shared<DHTSocketManager>(config);
    m_messageSender = std::make_shared<DHTMessageSender>(config, m_socketManager);
    m_routingManager = std::make_shared<DHTRoutingManager>(config, m_nodeID);
    m_peerStorage = std::make_shared<DHTPeerStorage>(config);
    m_transactionManager = std::make_shared<DHTTransactionManager>(config);
    m_nodeLookup = std::make_shared<DHTNodeLookup>(config, m_nodeID, m_routingManager, m_transactionManager);
    m_peerLookup = std::make_shared<DHTPeerLookup>(config, m_nodeID, m_routingManager, m_transactionManager, m_peerStorage, m_tokenManager);
    m_persistenceManager = std::make_shared<DHTPersistenceManager>(config, m_routingManager, m_peerStorage, m_transactionManager);

    // Create the message handler
    m_messageHandler = std::make_shared<DHTMessageHandler>(
        config,
        m_nodeID,
        m_messageSender,
        m_routingManager,
        m_transactionManager,
        m_peerStorage,
        m_tokenManager
    );

    // Ensure the config directory exists
    try {
        if (!std::filesystem::exists(config.getConfigDir())) {
            getLogger()->info("Config directory does not exist, creating: {}", config.getConfigDir());
            if (!std::filesystem::create_directories(config.getConfigDir())) {
                getLogger()->error("Failed to create config directory: {}", config.getConfigDir());
            }
        }
    } catch (const std::exception& e) {
        getLogger()->error("Exception while ensuring config directory exists: {}", e.what());
    }

    // First try to load the node ID from the routing table file
    std::string nodeIdFilePath = m_routingTablePath + ".nodeid";
    bool nodeIdLoaded = false;

    // Try to load from the nodeid file
    std::ifstream nodeIdInFile(nodeIdFilePath, std::ios::binary);
    if (nodeIdInFile.is_open()) {
        NodeID storedNodeId;
        if (nodeIdInFile.read(reinterpret_cast<char*>(storedNodeId.data()), static_cast<std::streamsize>(storedNodeId.size()))) {
            // Update our node ID to match the stored one
            m_nodeID = storedNodeId;
            getLogger()->info("Loaded node ID from file: {}", nodeIDToString(m_nodeID));
            nodeIdLoaded = true;
        }
        nodeIdInFile.close();
    }

    // If we couldn't load the node ID, save the new one
    if (!nodeIdLoaded) {
        // Save the node ID to a file
        std::ofstream nodeIdOutFile(nodeIdFilePath, std::ios::binary);
        if (nodeIdOutFile.is_open()) {
            nodeIdOutFile.write(reinterpret_cast<const char*>(m_nodeID.data()), static_cast<std::streamsize>(m_nodeID.size()));
            nodeIdOutFile.close();
            getLogger()->info("Saved new node ID to file: {}", nodeIdFilePath);
        } else {
            getLogger()->error("Failed to save node ID to file: {}", nodeIdFilePath);
        }
    }
}

DHTNode::DHTNode(uint16_t port, const std::string& configDir, const NodeID& nodeID)
    : DHTNode(DHTNodeConfig(), nodeID) {
    // Legacy constructor delegates to the new constructor
    // Set the port and config directory after construction
    m_port = port;
    m_config.setConfigDir(configDir);
    m_routingTablePath = m_config.getRoutingTablePath();
    m_peerCachePath = m_config.getPeerCachePath();
}

DHTNode::~DHTNode() {
    stop();
}

bool DHTNode::start() {
    if (m_running) {
        getLogger()->warning("DHT node already running");
        return false;
    }

    // Start the socket manager
    if (!m_socketManager->start([this](const uint8_t* data, size_t size, const network::EndPoint& sender) {
        // Handle received messages using the message handler
        if (m_messageHandler) {
            m_messageHandler->handleRawMessage(data, size, sender);
        }
    })) {
        getLogger()->error("Failed to start socket manager");
        return false;
    }

    // Start the message sender
    if (!m_messageSender->start()) {
        getLogger()->error("Failed to start message sender");
        m_socketManager->stop();
        return false;
    }

    // Start the routing manager
    if (!m_routingManager->start()) {
        getLogger()->error("Failed to start routing manager");
        m_messageSender->stop();
        m_socketManager->stop();
        return false;
    }

    // Start the peer storage
    if (!m_peerStorage->start()) {
        getLogger()->error("Failed to start peer storage");
        m_routingManager->stop();
        m_messageSender->stop();
        m_socketManager->stop();
        return false;
    }

    // Start the transaction manager
    if (!m_transactionManager->start()) {
        getLogger()->error("Failed to start transaction manager");
        m_peerStorage->stop();
        m_routingManager->stop();
        m_messageSender->stop();
        m_socketManager->stop();
        return false;
    }

    // Start the persistence manager
    if (!m_persistenceManager->start()) {
        getLogger()->error("Failed to start persistence manager");
        m_transactionManager->stop();
        m_peerStorage->stop();
        m_routingManager->stop();
        m_messageSender->stop();
        m_socketManager->stop();
        return false;
    }

    // Note: The node lookup and peer lookup managers don't need to be started explicitly

    // TODO: Start other component managers

    m_running = true;
    getLogger()->info("DHT node started on port {}", m_port);

    return true;
}

void DHTNode::stop() {
    // Use atomic operation to prevent multiple stops
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false)) {
        // Already stopped or stopping
        return;
    }

    getLogger()->info("Stopping DHT node");

    // Stop components in reverse order of initialization
    if (m_messageSender) {
        m_messageSender->stop();
    }

    if (m_socketManager) {
        m_socketManager->stop();
    }

    if (m_routingManager) {
        m_routingManager->stop();
    }

    if (m_peerStorage) {
        m_peerStorage->stop();
    }

    if (m_transactionManager) {
        m_transactionManager->stop();
    }

    if (m_persistenceManager) {
        m_persistenceManager->stop();
    }

    // TODO: Stop other component managers

    getLogger()->info("DHT node stopped");
}

bool DHTNode::isRunning() const {
    return m_running;
}

const NodeID& DHTNode::getNodeID() const {
    return m_nodeID;
}

uint16_t DHTNode::getPort() const {
    return m_port;
}

const RoutingTable& DHTNode::getRoutingTable() const {
    // For now, we still need to return the legacy routing table
    // In the future, this will return m_routingManager->getRoutingTable()
    return m_routingTable;
}

void DHTNode::setInfoHashCollector(std::shared_ptr<crawler::InfoHashCollector> collector) {
    m_infoHashCollector = collector;

    // Pass the collector to the message handler
    if (m_messageHandler) {
        m_messageHandler->setInfoHashCollector(collector);
    }

    getLogger()->info("InfoHash collector set");
}

std::shared_ptr<crawler::InfoHashCollector> DHTNode::getInfoHashCollector() const {
    return m_infoHashCollector;
}

bool DHTNode::bootstrap(const network::EndPoint& endpoint) {
    if (!m_running) {
        getLogger()->error("Cannot bootstrap: DHT node not running");
        return false;
    }

    getLogger()->info("Bootstrapping DHT node using {}", endpoint.toString());

    // TODO: Implement bootstrap using component managers

    return false; // Placeholder
}

bool DHTNode::bootstrap(const std::vector<network::EndPoint>& endpoints) {
    if (endpoints.empty()) {
        getLogger()->error("Cannot bootstrap: No endpoints provided");
        return false;
    }

    bool success = false;
    for (const auto& endpoint : endpoints) {
        if (bootstrap(endpoint)) {
            success = true;
        }
    }

    return success;
}

bool DHTNode::ping(const network::EndPoint& endpoint,
                  ResponseCallback responseCallback,
                  ErrorCallback errorCallback,
                  TimeoutCallback timeoutCallback) {
    if (!m_running) {
        getLogger()->error("Cannot ping: DHT node not running");
        return false;
    }

    if (!m_messageSender || !m_transactionManager) {
        getLogger()->error("Cannot ping: Required components not available");
        return false;
    }

    // Create a ping query
    auto query = std::make_shared<PingQuery>();
    query->setNodeID(m_nodeID);

    // Create a transaction
    std::string transactionID = m_transactionManager->createTransaction(
        query,
        endpoint,
        responseCallback,
        errorCallback,
        timeoutCallback
    );

    if (transactionID.empty()) {
        getLogger()->error("Failed to create transaction for ping to {}", endpoint.toString());
        return false;
    }

    // Send the query
    return m_messageSender->sendQuery(query, endpoint);
}

bool DHTNode::findNode(const NodeID& targetID,
                      const network::EndPoint& endpoint,
                      ResponseCallback responseCallback,
                      ErrorCallback errorCallback,
                      TimeoutCallback timeoutCallback) {
    if (!m_running) {
        getLogger()->error("Cannot find_node: DHT node not running");
        return false;
    }

    if (!m_messageSender || !m_transactionManager) {
        getLogger()->error("Cannot find_node: Required components not available");
        return false;
    }

    // Create a find_node query
    auto query = std::make_shared<FindNodeQuery>(targetID);
    query->setNodeID(m_nodeID);

    // Create a transaction
    std::string transactionID = m_transactionManager->createTransaction(
        query,
        endpoint,
        responseCallback,
        errorCallback,
        timeoutCallback
    );

    if (transactionID.empty()) {
        getLogger()->error("Failed to create transaction for find_node to {}", endpoint.toString());
        return false;
    }

    // Send the query
    return m_messageSender->sendQuery(query, endpoint);
}

bool DHTNode::getPeers(const InfoHash& infoHash,
                      const network::EndPoint& endpoint,
                      ResponseCallback responseCallback,
                      ErrorCallback errorCallback,
                      TimeoutCallback timeoutCallback) {
    if (!m_running) {
        getLogger()->error("Cannot get_peers: DHT node not running");
        return false;
    }

    if (!m_messageSender || !m_transactionManager) {
        getLogger()->error("Cannot get_peers: Required components not available");
        return false;
    }

    // Create a get_peers query
    auto query = std::make_shared<GetPeersQuery>(infoHash);
    query->setNodeID(m_nodeID);

    // Create a transaction
    std::string transactionID = m_transactionManager->createTransaction(
        query,
        endpoint,
        responseCallback,
        errorCallback,
        timeoutCallback
    );

    if (transactionID.empty()) {
        getLogger()->error("Failed to create transaction for get_peers to {}", endpoint.toString());
        return false;
    }

    // Send the query
    return m_messageSender->sendQuery(query, endpoint);
}

bool DHTNode::announcePeer(const InfoHash& infoHash,
                          uint16_t port,
                          const std::string& token,
                          const network::EndPoint& endpoint,
                          ResponseCallback responseCallback,
                          ErrorCallback errorCallback,
                          TimeoutCallback timeoutCallback) {
    if (!m_running) {
        getLogger()->error("Cannot announce_peer: DHT node not running");
        return false;
    }

    if (!m_messageSender || !m_transactionManager) {
        getLogger()->error("Cannot announce_peer: Required components not available");
        return false;
    }

    // Create an announce_peer query
    auto query = std::make_shared<AnnouncePeerQuery>(infoHash, port);
    query->setNodeID(m_nodeID);
    query->setToken(token);

    // Create a transaction
    std::string transactionID = m_transactionManager->createTransaction(
        query,
        endpoint,
        responseCallback,
        errorCallback,
        timeoutCallback
    );

    if (transactionID.empty()) {
        getLogger()->error("Failed to create transaction for announce_peer to {}", endpoint.toString());
        return false;
    }

    // Send the query
    return m_messageSender->sendQuery(query, endpoint);
}

bool DHTNode::findClosestNodes(const NodeID& targetID, NodeLookupCallback callback) {
    if (!m_running) {
        getLogger()->error("Cannot find closest nodes: DHT node not running");
        return false;
    }

    if (!m_nodeLookup) {
        getLogger()->error("Cannot find closest nodes: Node lookup manager not available");
        return false;
    }

    // Start a node lookup
    std::string lookupID = m_nodeLookup->startLookup(targetID, callback);

    if (lookupID.empty()) {
        getLogger()->error("Failed to start node lookup for target {}", nodeIDToString(targetID));
        return false;
    }

    getLogger()->debug("Started node lookup {} for target {}", lookupID, nodeIDToString(targetID));
    return true;
}

bool DHTNode::findPeers(const InfoHash& infoHash, GetPeersLookupCallback callback) {
    if (!m_running) {
        getLogger()->error("Cannot find peers: DHT node not running");
        return false;
    }

    if (!m_peerLookup) {
        getLogger()->error("Cannot find peers: Peer lookup manager not available");
        return false;
    }

    // Start a peer lookup
    std::string lookupID = m_peerLookup->startLookup(infoHash, callback);

    if (lookupID.empty()) {
        getLogger()->error("Failed to start peer lookup for info hash {}", infoHashToString(infoHash));
        return false;
    }

    getLogger()->debug("Started peer lookup {} for info hash {}", lookupID, infoHashToString(infoHash));
    return true;
}

bool DHTNode::announceAsPeer(const InfoHash& infoHash, uint16_t port, std::function<void(bool)> callback) {
    if (!m_running) {
        getLogger()->error("Cannot announce as peer: DHT node not running");
        return false;
    }

    if (!m_peerLookup) {
        getLogger()->error("Cannot announce as peer: Peer lookup manager not available");
        return false;
    }

    // Announce as a peer
    return m_peerLookup->announceAsPeer(infoHash, port, callback);
}

bool DHTNode::storePeer(const InfoHash& infoHash, const network::EndPoint& endpoint) {
    if (!m_peerStorage) {
        getLogger()->error("Cannot store peer: No peer storage available");
        return false;
    }

    return m_peerStorage->storePeer(infoHash, endpoint);
}

std::vector<network::EndPoint> DHTNode::getStoredPeers(const InfoHash& infoHash) const {
    if (!m_peerStorage) {
        getLogger()->error("Cannot get stored peers: No peer storage available");
        return {};
    }

    return m_peerStorage->getStoredPeers(infoHash);
}

size_t DHTNode::getStoredPeerCount(const InfoHash& infoHash) const {
    if (!m_peerStorage) {
        getLogger()->error("Cannot get stored peer count: No peer storage available");
        return 0;
    }

    return m_peerStorage->getStoredPeerCount(infoHash);
}

size_t DHTNode::getTotalStoredPeerCount() const {
    if (!m_peerStorage) {
        getLogger()->error("Cannot get total stored peer count: No peer storage available");
        return 0;
    }

    return m_peerStorage->getTotalStoredPeerCount();
}

size_t DHTNode::getInfoHashCount() const {
    if (!m_peerStorage) {
        getLogger()->error("Cannot get info hash count: No peer storage available");
        return 0;
    }

    return m_peerStorage->getInfoHashCount();
}

bool DHTNode::saveRoutingTable(const std::string& filePath) const {
    if (!m_persistenceManager) {
        getLogger()->error("Cannot save routing table: Persistence manager not available");
        return false;
    }

    return m_persistenceManager->saveRoutingTable(filePath);
}

bool DHTNode::loadRoutingTable(const std::string& filePath) {
    if (!m_persistenceManager) {
        getLogger()->error("Cannot load routing table: Persistence manager not available");
        return false;
    }

    return m_persistenceManager->loadRoutingTable(filePath);
}

bool DHTNode::loadNodeID(const std::string& filePath) {
    if (!m_persistenceManager) {
        getLogger()->error("Cannot load node ID: Persistence manager not available");
        return false;
    }

    return m_persistenceManager->loadNodeID(filePath, m_nodeID);
}

bool DHTNode::savePeerCache(const std::string& filePath) const {
    if (!m_peerStorage) {
        getLogger()->error("Cannot save peer cache: No peer storage available");
        return false;
    }

    return m_peerStorage->savePeerCache(filePath);
}

bool DHTNode::loadPeerCache(const std::string& filePath) {
    if (!m_peerStorage) {
        getLogger()->error("Cannot load peer cache: No peer storage available");
        return false;
    }

    return m_peerStorage->loadPeerCache(filePath);
}

bool DHTNode::saveNodeID(const std::string& filePath) const {
    if (!m_persistenceManager) {
        getLogger()->error("Cannot save node ID: Persistence manager not available");
        return false;
    }

    return m_persistenceManager->saveNodeID(filePath, m_nodeID);
}

bool DHTNode::performRandomNodeLookup() {
    if (!m_running) {
        getLogger()->warning("Cannot perform random node lookup: DHT node not running");
        return false;
    }

    // TODO: Implement performRandomNodeLookup using component managers

    return false; // Placeholder
}

} // namespace dht_hunter::dht
