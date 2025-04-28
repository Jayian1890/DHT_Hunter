#include "dht_hunter/dht/core/dht_node.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"

#include "dht_hunter/dht/bootstrap/bootstrapper.hpp"
#include "dht_hunter/dht/extensions/azureus_dht.hpp"
#include "dht_hunter/dht/extensions/kademlia_dht.hpp"
#include "dht_hunter/dht/extensions/mainline_dht.hpp"
#include "dht_hunter/dht/network/message_handler.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include "dht_hunter/dht/network/socket_manager.hpp"
#include "dht_hunter/dht/node_lookup/node_lookup.hpp"
#include "dht_hunter/dht/peer_lookup/peer_lookup.hpp"
#include "dht_hunter/dht/routing/routing_manager.hpp"
#include "dht_hunter/dht/storage/peer_storage.hpp"
#include "dht_hunter/dht/storage/token_manager.hpp"
#include "dht_hunter/dht/transactions/transaction_manager.hpp"
#include "dht_hunter/dht/crawler/crawler.hpp"
#include "dht_hunter/unified_event/events/custom_events.hpp"

#include <iomanip>

namespace dht_hunter::dht {

// Use the existing infoHashToString function from dht_types.hpp

DHTNode::DHTNode(const DHTConfig& config) : m_nodeID(generateRandomNodeID()), m_config(config), m_running(false) {
    // TRACE: Constructor entry
    unified_event::logTrace("DHT.Node", "TRACE: Constructor entry");

    // Log node initialization
    unified_event::logDebug("DHT.Node", "Initializing node with ID: " + nodeIDToString(m_nodeID));

    // Create the components
    unified_event::logTrace("DHT.Node", "TRACE: Getting RoutingTable instance");
    m_routingTable = RoutingTable::getInstance(m_nodeID, config.getKBucketSize());

    unified_event::logTrace("DHT.Node", "TRACE: Getting SocketManager instance");
    m_socketManager = SocketManager::getInstance(config);

    unified_event::logTrace("DHT.Node", "TRACE: Getting MessageSender instance");
    m_messageSender = MessageSender::getInstance(config, m_socketManager);

    unified_event::logTrace("DHT.Node", "TRACE: Getting TokenManager instance");
    m_tokenManager = TokenManager::getInstance(config);

    unified_event::logTrace("DHT.Node", "TRACE: Getting PeerStorage instance");
    m_peerStorage = PeerStorage::getInstance(config);
    unified_event::logTrace("DHT.Node", "TRACE: Getting TransactionManager instance");
    m_transactionManager = TransactionManager::getInstance(config);

    unified_event::logTrace("DHT.Node", "TRACE: Getting RoutingManager instance");
    m_routingManager = RoutingManager::getInstance(config, m_nodeID, m_routingTable, m_transactionManager, m_messageSender);

    unified_event::logTrace("DHT.Node", "TRACE: Getting NodeLookup instance");
    m_nodeLookup = NodeLookup::getInstance(config, m_nodeID, m_routingTable, m_transactionManager, m_messageSender);

    unified_event::logTrace("DHT.Node", "TRACE: Getting PeerLookup instance");
    m_peerLookup = PeerLookup::getInstance(config, m_nodeID, m_routingTable, m_transactionManager, m_messageSender, m_tokenManager, m_peerStorage);

    unified_event::logTrace("DHT.Node", "TRACE: Getting Bootstrapper instance");
    m_bootstrapper = Bootstrapper::getInstance(config, m_nodeID, m_routingManager, m_nodeLookup);

    unified_event::logTrace("DHT.Node", "TRACE: Getting MessageHandler instance");
    m_messageHandler = MessageHandler::getInstance(config, m_nodeID, m_messageSender, m_routingTable, m_tokenManager, m_peerStorage, m_transactionManager);

    unified_event::logTrace("DHT.Node", "TRACE: Getting BTMessageHandler instance");
    m_btMessageHandler = bittorrent::BTMessageHandler::getInstance(m_routingManager);

    // Get the event bus
    unified_event::logTrace("DHT.Node", "TRACE: Getting EventBus instance");
    m_eventBus = unified_event::EventBus::getInstance();

    // Get the statistics service
    unified_event::logTrace("DHT.Node", "TRACE: Getting StatisticsService instance");
    m_statisticsService = services::StatisticsService::getInstance();

    // Create DHT extensions
    unified_event::logTrace("DHT.Node", "TRACE: Creating MainlineDHT extension");
    m_mainlineDHT = std::make_shared<extensions::MainlineDHT>(config, m_nodeID, m_routingTable);

    unified_event::logTrace("DHT.Node", "TRACE: Creating KademliaDHT extension");
    m_kademliaDHT = std::make_shared<extensions::KademliaDHT>(config, m_nodeID, m_routingTable);

    unified_event::logTrace("DHT.Node", "TRACE: Creating AzureusDHT extension");
    m_azureusDHT = std::make_shared<extensions::AzureusDHT>(config, m_nodeID, m_routingTable);

    // Create the crawler
    unified_event::logTrace("DHT.Node", "TRACE: Getting Crawler instance");
    m_crawler = Crawler::getInstance(config, m_nodeID, m_routingManager, m_nodeLookup, m_peerLookup, m_transactionManager, m_messageSender, m_peerStorage);

    unified_event::logTrace("DHT.Node", "TRACE: Constructor exit");

    // Log DHT node creation
    unified_event::logInfo("DHT.Node", "Node initialized with ID: " + nodeIDToString(m_nodeID));
}

DHTNode::~DHTNode() {
    unified_event::logTrace("DHT.Node", "TRACE: Destructor entry");
    stop();
    unified_event::logTrace("DHT.Node", "TRACE: Destructor exit");
}

bool DHTNode::start() {
    unified_event::logTrace("DHT.Node", "TRACE: start() entry");

    // Check if already running
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        // Already running
        unified_event::logTrace("DHT.Node", "TRACE: Already running, returning");
        return true;
    }

    // Start socket manager
    unified_event::logDebug("DHT.Node", "Starting Socket Manager");
    unified_event::logTrace("DHT.Node", "TRACE: Starting SocketManager");

    if (!m_socketManager->start([this](const uint8_t* data, size_t size, const network::EndPoint& sender) {
            unified_event::logTrace("DHT.Node", "TRACE: Received raw message from " + sender.toString());
            if (m_messageHandler) {
                unified_event::logTrace("DHT.Node", "TRACE: Forwarding to MessageHandler");
                m_messageHandler->handleRawMessage(data, size, sender);
            }
        })) {
        unified_event::logError("DHT.Node", "Failed to start socket manager");
        unified_event::logTrace("DHT.Node", "TRACE: SocketManager failed to start, returning false");
        return false;
    }

    unified_event::logTrace("DHT.Node", "TRACE: SocketManager started successfully");

    // Start message sender
    unified_event::logDebug("DHT.Node", "Starting Message Sender");
    unified_event::logTrace("DHT.Node", "TRACE: Starting MessageSender");

    if (!m_messageSender->start()) {
        unified_event::logError("DHT.Node", "Failed to start message sender");
        unified_event::logTrace("DHT.Node", "TRACE: MessageSender failed to start, stopping SocketManager");
        m_socketManager->stop();
        unified_event::logTrace("DHT.Node", "TRACE: Returning false");
        return false;
    }

    unified_event::logTrace("DHT.Node", "TRACE: MessageSender started successfully");

    // Start token manager
    unified_event::logDebug("DHT.Node", "Starting Token Manager");
    unified_event::logTrace("DHT.Node", "TRACE: Starting TokenManager");

    if (!m_tokenManager->start()) {
        unified_event::logError("DHT.Node", "Failed to start token manager");
        unified_event::logTrace("DHT.Node", "TRACE: TokenManager failed to start, stopping MessageSender and SocketManager");
        m_messageSender->stop();
        m_socketManager->stop();
        unified_event::logTrace("DHT.Node", "TRACE: Returning false");
        return false;
    }

    unified_event::logTrace("DHT.Node", "TRACE: TokenManager started successfully");

    // Start peer storage
    unified_event::logDebug("DHT.Node", "Starting Peer Storage");
    unified_event::logTrace("DHT.Node", "TRACE: Starting PeerStorage");

    if (!m_peerStorage->start()) {
        unified_event::logError("DHT.Node", "Failed to start peer storage");
        unified_event::logTrace("DHT.Node", "TRACE: PeerStorage failed to start, stopping TokenManager, MessageSender, and SocketManager");
        m_tokenManager->stop();
        m_messageSender->stop();
        m_socketManager->stop();
        unified_event::logTrace("DHT.Node", "TRACE: Returning false");
        return false;
    }

    unified_event::logTrace("DHT.Node", "TRACE: PeerStorage started successfully");

    // Start transaction manager
    unified_event::logDebug("DHT.Node", "Starting Transaction Manager");
    unified_event::logTrace("DHT.Node", "TRACE: Starting TransactionManager");

    if (!m_transactionManager->start()) {
        unified_event::logError("DHT.Node", "Failed to start transaction manager");
        unified_event::logTrace("DHT.Node", "TRACE: TransactionManager failed to start, stopping PeerStorage, TokenManager, MessageSender, and SocketManager");
        m_peerStorage->stop();
        m_tokenManager->stop();
        m_messageSender->stop();
        m_socketManager->stop();
        unified_event::logTrace("DHT.Node", "TRACE: Returning false");
        return false;
    }

    unified_event::logTrace("DHT.Node", "TRACE: TransactionManager started successfully");

    // Start routing manager
    unified_event::logDebug("DHT.Node", "Starting Routing Manager");
    unified_event::logTrace("DHT.Node", "TRACE: Starting RoutingManager");

    if (!m_routingManager->start()) {
        unified_event::logError("DHT.Node", "Failed to start routing manager");
        unified_event::logTrace("DHT.Node", "TRACE: RoutingManager failed to start, stopping TransactionManager, PeerStorage, TokenManager, MessageSender, and SocketManager");
        m_transactionManager->stop();
        m_peerStorage->stop();
        m_tokenManager->stop();
        m_messageSender->stop();
        m_socketManager->stop();
        unified_event::logTrace("DHT.Node", "TRACE: Returning false");
        return false;
    }

    unified_event::logTrace("DHT.Node", "TRACE: RoutingManager started successfully");

    // Initialize DHT extensions
    unified_event::logDebug("DHT.Node", "Initializing Mainline DHT Extension");
    unified_event::logTrace("DHT.Node", "TRACE: Initializing MainlineDHT extension");

    if (m_mainlineDHT && !m_mainlineDHT->initialize()) {
        unified_event::logError("DHT.Node", "Failed to initialize Mainline DHT extension");
        unified_event::logTrace("DHT.Node", "TRACE: MainlineDHT initialization failed, resetting");
        m_mainlineDHT.reset();
    } else if (m_mainlineDHT) {
        unified_event::logDebug("DHT.Node", "Mainline DHT Extension Initialized");
        unified_event::logTrace("DHT.Node", "TRACE: MainlineDHT initialized successfully");
    }

    unified_event::logDebug("DHT.Node", "Initializing Kademlia DHT Extension");
    unified_event::logTrace("DHT.Node", "TRACE: Initializing KademliaDHT extension");

    if (m_kademliaDHT && !m_kademliaDHT->initialize()) {
        unified_event::logError("DHT.Node", "Failed to initialize Kademlia DHT extension");
        unified_event::logTrace("DHT.Node", "TRACE: KademliaDHT initialization failed, resetting");
        m_kademliaDHT.reset();
    } else if (m_kademliaDHT) {
        unified_event::logDebug("DHT.Node", "Kademlia DHT Extension Initialized");
        unified_event::logTrace("DHT.Node", "TRACE: KademliaDHT initialized successfully");
    }

    unified_event::logDebug("DHT.Node", "Initializing Azureus DHT Extension");
    unified_event::logTrace("DHT.Node", "TRACE: Initializing AzureusDHT extension");

    if (m_azureusDHT && !m_azureusDHT->initialize()) {
        unified_event::logError("DHT.Node", "Failed to initialize Azureus DHT extension");
        unified_event::logTrace("DHT.Node", "TRACE: AzureusDHT initialization failed, resetting");
        m_azureusDHT.reset();
    } else if (m_azureusDHT) {
        unified_event::logDebug("DHT.Node", "Azureus DHT Extension Initialized");
        unified_event::logTrace("DHT.Node", "TRACE: AzureusDHT initialized successfully");
    }

    // m_running is already set to true by the compare_exchange_strong above

    // Subscribe to events
    unified_event::logTrace("DHT.Node", "TRACE: Subscribing to events");
    subscribeToEvents();
    unified_event::logTrace("DHT.Node", "TRACE: Subscribed to events");

    // Start the statistics service
    unified_event::logDebug("DHT.Node", "Starting Statistics Service");
    unified_event::logTrace("DHT.Node", "TRACE: Starting StatisticsService");

    if (m_statisticsService && !m_statisticsService->start()) {
        unified_event::logWarning("DHT.Node", "Failed to start Statistics Service");
        unified_event::logTrace("DHT.Node", "TRACE: StatisticsService failed to start, but continuing anyway");
        // Continue anyway, this is not critical
    } else if (m_statisticsService) {
        unified_event::logDebug("DHT.Node", "Statistics Service Started");
        unified_event::logTrace("DHT.Node", "TRACE: StatisticsService started successfully");
    }

    // Bootstrap the node
    unified_event::logDebug("DHT.Node", "Starting Bootstrap");
    unified_event::logTrace("DHT.Node", "TRACE: Starting bootstrap process");

    m_bootstrapper->bootstrap([this](const bool success) {
        unified_event::logTrace("DHT.Node", "TRACE: Bootstrap callback called with success=" + std::string(success ? "true" : "false"));
        if (success) {
            unified_event::logDebug("DHT.Node", "Bootstrap Completed, node count: " + std::to_string(m_routingTable->getNodeCount()));
            unified_event::logTrace("DHT.Node", "TRACE: Creating and publishing SystemStartedEvent");

            // Publish a system started event
            auto bootstrapEvent = std::make_shared<unified_event::SystemStartedEvent>("DHT.Node.Bootstrap");
            m_eventBus->publish(bootstrapEvent);
            unified_event::logTrace("DHT.Node", "TRACE: SystemStartedEvent published");
        } else {
            unified_event::logWarning("DHT.Node", "Bootstrap Failed");
            unified_event::logTrace("DHT.Node", "TRACE: Bootstrap process failed");
        }
    });

    // Publish a system started event
    unified_event::logTrace("DHT.Node", "TRACE: Creating and publishing DHT.Node SystemStartedEvent");
    auto startedEvent = std::make_shared<unified_event::SystemStartedEvent>("DHT.Node");
    m_eventBus->publish(startedEvent);
    unified_event::logTrace("DHT.Node", "TRACE: DHT.Node SystemStartedEvent published");

    unified_event::logTrace("DHT.Node", "TRACE: start() returning true");
    return true;
}

void DHTNode::stop() {
    unified_event::logTrace("DHT.Node", "TRACE: stop() entry");

    // Check if already stopped
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
        // Already stopped
        unified_event::logDebug("DHT.Node", "Stop called while not running");
        unified_event::logTrace("DHT.Node", "TRACE: Already stopped, returning");
        return;
    }

    unified_event::logDebug("DHT.Node", "Stopping DHT Node");
    unified_event::logTrace("DHT.Node", "TRACE: Beginning shutdown sequence");

    // Stop the DHT extensions
    unified_event::logDebug("DHT.Node", "Stopping DHT Extensions");
    unified_event::logTrace("DHT.Node", "TRACE: Stopping DHT extensions");

    if (m_azureusDHT) {
        unified_event::logDebug("DHT.Node", "Stopping Azureus DHT");
        unified_event::logTrace("DHT.Node", "TRACE: Stopping AzureusDHT");
        m_azureusDHT->shutdown();
        unified_event::logTrace("DHT.Node", "TRACE: AzureusDHT stopped");
    }
    if (m_kademliaDHT) {
        unified_event::logDebug("DHT.Node", "Stopping Kademlia DHT");
        unified_event::logTrace("DHT.Node", "TRACE: Stopping KademliaDHT");
        m_kademliaDHT->shutdown();
        unified_event::logTrace("DHT.Node", "TRACE: KademliaDHT stopped");
    }
    if (m_mainlineDHT) {
        unified_event::logDebug("DHT.Node", "Stopping Mainline DHT");
        unified_event::logTrace("DHT.Node", "TRACE: Stopping MainlineDHT");
        m_mainlineDHT->shutdown();
        unified_event::logTrace("DHT.Node", "TRACE: MainlineDHT stopped");
    }

    // Stop the components in reverse order
    unified_event::logDebug("DHT.Node", "Stopping Components");
    unified_event::logTrace("DHT.Node", "TRACE: Stopping components in reverse order");

    unified_event::logDebug("DHT.Node", "Stopping Routing Manager");
    unified_event::logTrace("DHT.Node", "TRACE: Stopping RoutingManager");
    m_routingManager->stop();
    unified_event::logTrace("DHT.Node", "TRACE: RoutingManager stopped");

    unified_event::logDebug("DHT.Node", "Stopping Transaction Manager");
    unified_event::logTrace("DHT.Node", "TRACE: Stopping TransactionManager");
    m_transactionManager->stop();
    unified_event::logTrace("DHT.Node", "TRACE: TransactionManager stopped");

    unified_event::logDebug("DHT.Node", "Stopping Peer Storage");
    unified_event::logTrace("DHT.Node", "TRACE: Stopping PeerStorage");
    m_peerStorage->stop();
    unified_event::logTrace("DHT.Node", "TRACE: PeerStorage stopped");

    unified_event::logDebug("DHT.Node", "Stopping Token Manager");
    unified_event::logTrace("DHT.Node", "TRACE: Stopping TokenManager");
    m_tokenManager->stop();
    unified_event::logTrace("DHT.Node", "TRACE: TokenManager stopped");

    unified_event::logDebug("DHT.Node", "Stopping Message Sender");
    unified_event::logTrace("DHT.Node", "TRACE: Stopping MessageSender");
    m_messageSender->stop();
    unified_event::logTrace("DHT.Node", "TRACE: MessageSender stopped");

    unified_event::logDebug("DHT.Node", "Stopping Socket Manager");
    unified_event::logTrace("DHT.Node", "TRACE: Stopping SocketManager");
    m_socketManager->stop();
    unified_event::logTrace("DHT.Node", "TRACE: SocketManager stopped");

    // Unsubscribe from events
    unified_event::logTrace("DHT.Node", "TRACE: Unsubscribing from events");
    for (int subscriptionId : m_eventSubscriptionIds) {
        m_eventBus->unsubscribe(subscriptionId);
    }
    m_eventSubscriptionIds.clear();
    unified_event::logTrace("DHT.Node", "TRACE: Unsubscribed from all events");

    // Stop the statistics service
    if (m_statisticsService) {
        unified_event::logDebug("DHT.Node", "Stopping Statistics Service");
        unified_event::logTrace("DHT.Node", "TRACE: Stopping StatisticsService");
        m_statisticsService->stop();
        unified_event::logTrace("DHT.Node", "TRACE: StatisticsService stopped");
    }

    // Publish a system stopped event
    unified_event::logTrace("DHT.Node", "TRACE: Creating and publishing DHT.Node SystemStoppedEvent");
    auto stoppedEvent = std::make_shared<unified_event::SystemStoppedEvent>("DHT.Node");
    m_eventBus->publish(stoppedEvent);
    unified_event::logTrace("DHT.Node", "TRACE: DHT.Node SystemStoppedEvent published");

    // m_running is already set to false by the compare_exchange_strong above
    unified_event::logTrace("DHT.Node", "TRACE: stop() exit");
}

bool DHTNode::isRunning() const {
    unified_event::logTrace("DHT.Node", "TRACE: isRunning() called");
    bool running = m_running.load(std::memory_order_acquire);
    unified_event::logTrace("DHT.Node", "TRACE: isRunning() returning " + std::string(running ? "true" : "false"));
    return running;
}

const NodeID& DHTNode::getNodeID() const {
    unified_event::logTrace("DHT.Node", "TRACE: getNodeID() called");
    unified_event::logTrace("DHT.Node", "TRACE: getNodeID() returning " + nodeIDToString(m_nodeID));
    return m_nodeID;
}

uint16_t DHTNode::getPort() const {
    unified_event::logTrace("DHT.Node", "TRACE: getPort() called");
    uint16_t port = m_config.getPort();
    unified_event::logTrace("DHT.Node", "TRACE: getPort() returning " + std::to_string(port));
    return port;
}

std::string DHTNode::getStatistics() const {
    unified_event::logTrace("DHT.Node", "TRACE: getStatistics() called");
    if (m_statisticsService) {
        unified_event::logTrace("DHT.Node", "TRACE: Getting statistics from StatisticsService");
        std::string stats = m_statisticsService->getStatisticsAsJson();
        unified_event::logTrace("DHT.Node", "TRACE: getStatistics() returning JSON data");
        return stats;
    }
    unified_event::logTrace("DHT.Node", "TRACE: StatisticsService not available, returning empty JSON");
    return "{}";
}

bool DHTNode::handlePortMessage(const network::NetworkAddress& peerAddress, const uint8_t* data, size_t length) const {
    unified_event::logTrace("DHT.Node", "TRACE: handlePortMessage(data, length) called for peer " + peerAddress.toString());
    if (!m_running || !m_btMessageHandler) {
        unified_event::logTrace("DHT.Node", "TRACE: Node not running or BTMessageHandler not available, returning false");
        return false;
    }

    unified_event::logTrace("DHT.Node", "TRACE: Forwarding port message to BTMessageHandler");
    bool result = m_btMessageHandler->handlePortMessage(peerAddress, data, length);
    unified_event::logTrace("DHT.Node", "TRACE: handlePortMessage(data, length) returning " + std::string(result ? "true" : "false"));
    return result;
}

bool DHTNode::handlePortMessage(const network::NetworkAddress& peerAddress, uint16_t port) const {
    unified_event::logTrace("DHT.Node", "TRACE: handlePortMessage(port) called for peer " + peerAddress.toString() + " with port " + std::to_string(port));
    if (!m_running || !m_btMessageHandler) {
        unified_event::logTrace("DHT.Node", "TRACE: Node not running or BTMessageHandler not available, returning false");
        return false;
    }

    unified_event::logTrace("DHT.Node", "TRACE: Forwarding port message to BTMessageHandler");
    bool result = m_btMessageHandler->handlePortMessage(peerAddress, port);
    unified_event::logTrace("DHT.Node", "TRACE: handlePortMessage(port) returning " + std::string(result ? "true" : "false"));
    return result;
}

std::vector<uint8_t> DHTNode::createPortMessage() const {
    unified_event::logTrace("DHT.Node", "TRACE: createPortMessage() called");
    uint16_t port = getPort();
    unified_event::logTrace("DHT.Node", "TRACE: Creating port message for port " + std::to_string(port));
    std::vector<uint8_t> message = bittorrent::BTMessageHandler::createPortMessage(port);
    unified_event::logTrace("DHT.Node", "TRACE: createPortMessage() returning message of size " + std::to_string(message.size()));
    return message;
}

std::shared_ptr<RoutingTable> DHTNode::getRoutingTable() const {
    unified_event::logTrace("DHT.Node", "TRACE: getRoutingTable() called");
    unified_event::logTrace("DHT.Node", "TRACE: getRoutingTable() returning routing table with " + std::to_string(m_routingTable->getNodeCount()) + " nodes");
    return m_routingTable;
}

void DHTNode::findClosestNodes(const NodeID& targetID, const std::function<void(const std::vector<std::shared_ptr<Node>>&)>& callback) const {
    unified_event::logTrace("DHT.Node", "TRACE: findClosestNodes() called for target ID: " + nodeIDToString(targetID));
    unified_event::logDebug("DHT.Node", "Find Closest Nodes for target ID: " + nodeIDToString(targetID));

    if (!m_running) {
        unified_event::logError("DHT.Node", "Find Closest Nodes called while not running");
        unified_event::logTrace("DHT.Node", "TRACE: Node not running, returning empty result");
        if (callback) {
            callback({});
        }
        return;
    }

    if (!m_nodeLookup) {
        unified_event::logError("DHT.Node", "No node lookup available");
        unified_event::logTrace("DHT.Node", "TRACE: NodeLookup not available, returning empty result");
        if (callback) {
            callback({});
        }
        return;
    }
    unified_event::logTrace("DHT.Node", "TRACE: Forwarding findClosestNodes request to NodeLookup");

    m_nodeLookup->lookup(targetID, [targetID, callback](const std::vector<std::shared_ptr<Node>>& nodes) {
        unified_event::logTrace("DHT.Node", "TRACE: findClosestNodes callback received " + std::to_string(nodes.size()) + " nodes");
        unified_event::logDebug("DHT.Node", "Find Closest Nodes Result for target ID: " + nodeIDToString(targetID) + ", found " + std::to_string(nodes.size()) + " nodes");

        if (callback) {
            unified_event::logTrace("DHT.Node", "TRACE: Calling user callback with results");
            callback(nodes);
        } else {
            unified_event::logTrace("DHT.Node", "TRACE: No user callback provided");
        }
        unified_event::logTrace("DHT.Node", "TRACE: findClosestNodes callback completed");
    });
    unified_event::logTrace("DHT.Node", "TRACE: findClosestNodes() lookup initiated");
}

void DHTNode::getPeers(const InfoHash& infoHash, const std::function<void(const std::vector<network::EndPoint>&)>& callback) const {
    unified_event::logTrace("DHT.Node", "TRACE: getPeers() called for info hash: " + infoHashToString(infoHash));
    unified_event::logDebug("DHT.Node", "Get Peers for info hash: " + infoHashToString(infoHash));

    if (!m_running) {
        unified_event::logError("DHT.Node", "Get Peers called while not running");
        unified_event::logTrace("DHT.Node", "TRACE: Node not running, returning empty result");
        if (callback) {
            callback({});
        }
        return;
    }

    if (!m_peerLookup) {
        unified_event::logError("DHT.Node", "No peer lookup available");
        unified_event::logTrace("DHT.Node", "TRACE: PeerLookup not available, returning empty result");
        if (callback) {
            callback({});
        }
        return;
    }
    unified_event::logTrace("DHT.Node", "TRACE: Forwarding getPeers request to PeerLookup");

    m_peerLookup->lookup(infoHash, [infoHash, callback](const std::vector<network::EndPoint>& peers) {
        unified_event::logTrace("DHT.Node", "TRACE: getPeers callback received " + std::to_string(peers.size()) + " peers");
        unified_event::logDebug("DHT.Node", "Get Peers Result for info hash: " + infoHashToString(infoHash) + ", found " + std::to_string(peers.size()) + " peers");

        if (callback) {
            unified_event::logTrace("DHT.Node", "TRACE: Calling user callback with results");
            callback(peers);
        } else {
            unified_event::logTrace("DHT.Node", "TRACE: No user callback provided");
        }
        unified_event::logTrace("DHT.Node", "TRACE: getPeers callback completed");
    });
    unified_event::logTrace("DHT.Node", "TRACE: getPeers() lookup initiated");
}

void DHTNode::announcePeer(const InfoHash& infoHash, const uint16_t port, const std::function<void(bool)>& callback) const {
    unified_event::logTrace("DHT.Node", "TRACE: announcePeer() called for info hash: " + infoHashToString(infoHash) + ", port: " + std::to_string(port));
    unified_event::logDebug("DHT.Node", "Announce Peer for info hash: " + infoHashToString(infoHash) + ", port: " + std::to_string(port));

    if (!m_running) {
        unified_event::logError("DHT.Node", "Announce Peer called while not running");
        unified_event::logTrace("DHT.Node", "TRACE: Node not running, returning failure");
        if (callback) {
            callback(false);
        }
        return;
    }

    if (!m_peerLookup) {
        unified_event::logError("DHT.Node", "No peer lookup available");
        unified_event::logTrace("DHT.Node", "TRACE: PeerLookup not available, returning failure");
        if (callback) {
            callback(false);
        }
        return;
    }
    unified_event::logTrace("DHT.Node", "TRACE: Forwarding announcePeer request to PeerLookup");

    m_peerLookup->announce(infoHash, port, [infoHash, port, callback](bool success) {
        unified_event::logTrace("DHT.Node", "TRACE: announcePeer callback received success=" + std::string(success ? "true" : "false"));
        unified_event::logDebug("DHT.Node", "Announce Peer Result for info hash: " + infoHashToString(infoHash) + ", port: " + std::to_string(port) + ", success: " + (success ? "true" : "false"));

        if (callback) {
            unified_event::logTrace("DHT.Node", "TRACE: Calling user callback with result");
            callback(success);
        } else {
            unified_event::logTrace("DHT.Node", "TRACE: No user callback provided");
        }
        unified_event::logTrace("DHT.Node", "TRACE: announcePeer callback completed");
    });
    unified_event::logTrace("DHT.Node", "TRACE: announcePeer() announce initiated");
}

void DHTNode::ping(const std::shared_ptr<Node>& node, const std::function<void(bool)>& callback) {
    unified_event::logTrace("DHT.Node", "TRACE: ping() called for node ID: " + nodeIDToString(node->getID()) + ", endpoint: " + node->getEndpoint().toString());
    unified_event::logDebug("DHT.Node", "Ping Node with ID: " + nodeIDToString(node->getID()) + ", endpoint: " + node->getEndpoint().toString());

    if (!m_running) {
        unified_event::logError("DHT.Node", "Ping called while not running");
        unified_event::logTrace("DHT.Node", "TRACE: Node not running, returning failure");
        if (callback) {
            callback(false);
        }
        return;
    }

    if (!m_messageSender || !m_transactionManager) {
        unified_event::logError("DHT.Node", "No message sender or transaction manager available");
        unified_event::logTrace("DHT.Node", "TRACE: MessageSender or TransactionManager not available, returning failure");
        if (callback) {
            callback(false);
        }
        return;
    }
    unified_event::logTrace("DHT.Node", "TRACE: Creating ping query");

    // Create a ping query
    unified_event::logTrace("DHT.Node", "TRACE: Creating PingQuery");
    auto query = std::make_shared<PingQuery>("", m_nodeID);
    unified_event::logTrace("DHT.Node", "TRACE: PingQuery created");

    // Send the query
    unified_event::logTrace("DHT.Node", "TRACE: Creating transaction for ping query");
    std::string transactionID = m_transactionManager->createTransaction(
        query, node->getEndpoint(),
        [node, callback](const std::shared_ptr<ResponseMessage>& /*response*/, const network::EndPoint& /*sender*/) {
            unified_event::logTrace("DHT.Node", "TRACE: Ping success callback called for node ID: " + nodeIDToString(node->getID()));
            unified_event::logDebug("DHT.Node", "Ping Success for node ID: " + nodeIDToString(node->getID()) + ", endpoint: " + node->getEndpoint().toString());

            if (callback) {
                unified_event::logTrace("DHT.Node", "TRACE: Calling user callback with success=true");
                callback(true);
            } else {
                unified_event::logTrace("DHT.Node", "TRACE: No user callback provided");
            }
            unified_event::logTrace("DHT.Node", "TRACE: Ping success callback completed");
        },
        [node, callback](const std::shared_ptr<ErrorMessage>& /*error*/, const network::EndPoint& /*sender*/) {
            unified_event::logTrace("DHT.Node", "TRACE: Ping error callback called for node ID: " + nodeIDToString(node->getID()));
            unified_event::logDebug("DHT.Node", "Ping Error for node ID: " + nodeIDToString(node->getID()) + ", endpoint: " + node->getEndpoint().toString());

            if (callback) {
                unified_event::logTrace("DHT.Node", "TRACE: Calling user callback with success=false");
                callback(false);
            } else {
                unified_event::logTrace("DHT.Node", "TRACE: No user callback provided");
            }
            unified_event::logTrace("DHT.Node", "TRACE: Ping error callback completed");
        },
        [node, callback]() {
            unified_event::logTrace("DHT.Node", "TRACE: Ping timeout callback called for node ID: " + nodeIDToString(node->getID()));
            unified_event::logDebug("DHT.Node", "Ping Timeout for node ID: " + nodeIDToString(node->getID()) + ", endpoint: " + node->getEndpoint().toString());

            if (callback) {
                unified_event::logTrace("DHT.Node", "TRACE: Calling user callback with success=false");
                callback(false);
            } else {
                unified_event::logTrace("DHT.Node", "TRACE: No user callback provided");
            }
            unified_event::logTrace("DHT.Node", "TRACE: Ping timeout callback completed");
        });
    unified_event::logTrace("DHT.Node", "TRACE: Transaction created with ID: " + (transactionID.empty() ? "<empty>" : transactionID));

    if (!transactionID.empty()) {
        unified_event::logTrace("DHT.Node", "TRACE: Transaction created successfully, sending ping query");
        unified_event::logDebug("DHT.Node", "Sending Ping to node ID: " + nodeIDToString(node->getID()) + ", endpoint: " + node->getEndpoint().toString() + ", transaction ID: " + transactionID);

        unified_event::logTrace("DHT.Node", "TRACE: Sending query via MessageSender");
        m_messageSender->sendQuery(query, node->getEndpoint());
        unified_event::logTrace("DHT.Node", "TRACE: Query sent");
    } else {
        unified_event::logTrace("DHT.Node", "TRACE: Failed to create transaction");
        unified_event::logError("DHT.Node", "Failed to create transaction for node ID: " + nodeIDToString(node->getID()) + ", endpoint: " + node->getEndpoint().toString());

        if (callback) {
            unified_event::logTrace("DHT.Node", "TRACE: Calling user callback with success=false");
            callback(false);
        } else {
            unified_event::logTrace("DHT.Node", "TRACE: No user callback provided");
        }
    }
    unified_event::logTrace("DHT.Node", "TRACE: ping() completed");
}

void DHTNode::subscribeToEvents() {
    unified_event::logTrace("DHT.Node", "TRACE: subscribeToEvents() entry");
    unified_event::logDebug("DHT.Node", "Subscribing to Events");

    // Subscribe to node discovered events
    unified_event::logTrace("DHT.Node", "TRACE: Subscribing to NodeDiscovered events");
    int nodeDiscoveredId = m_eventBus->subscribe(
        unified_event::EventType::NodeDiscovered,
        [this](const std::shared_ptr<unified_event::Event>& event) {
            unified_event::logTrace("DHT.Node", "TRACE: NodeDiscovered event received");
            this->handleNodeDiscoveredEvent(event);
        });
    m_eventSubscriptionIds.push_back(nodeDiscoveredId);
    unified_event::logTrace("DHT.Node", "TRACE: Subscribed to NodeDiscovered events with ID " + std::to_string(nodeDiscoveredId));

    // Subscribe to peer discovered events
    unified_event::logTrace("DHT.Node", "TRACE: Subscribing to PeerDiscovered events");
    int peerDiscoveredId = m_eventBus->subscribe(
        unified_event::EventType::PeerDiscovered,
        [this](const std::shared_ptr<unified_event::Event>& event) {
            unified_event::logTrace("DHT.Node", "TRACE: PeerDiscovered event received");
            this->handlePeerDiscoveredEvent(event);
        });
    m_eventSubscriptionIds.push_back(peerDiscoveredId);
    unified_event::logTrace("DHT.Node", "TRACE: Subscribed to PeerDiscovered events with ID " + std::to_string(peerDiscoveredId));

    // Subscribe to system error events
    unified_event::logTrace("DHT.Node", "TRACE: Subscribing to SystemError events");
    int systemErrorId = m_eventBus->subscribe(
        unified_event::EventType::SystemError,
        [this](const std::shared_ptr<unified_event::Event>& event) {
            unified_event::logTrace("DHT.Node", "TRACE: SystemError event received");
            this->handleSystemErrorEvent(event);
        });
    m_eventSubscriptionIds.push_back(systemErrorId);
    unified_event::logTrace("DHT.Node", "TRACE: Subscribed to SystemError events with ID " + std::to_string(systemErrorId));

    // Subscribe to message sent events
    unified_event::logTrace("DHT.Node", "TRACE: Subscribing to MessageSent events");
    int messageSentId = m_eventBus->subscribe(
        unified_event::EventType::MessageSent,
        [](const std::shared_ptr<unified_event::Event>&) {
            unified_event::logTrace("DHT.Node", "TRACE: MessageSent event received");
            // No special handling needed, just let the logging processor handle it
        });
    m_eventSubscriptionIds.push_back(messageSentId);
    unified_event::logTrace("DHT.Node", "TRACE: Subscribed to MessageSent events with ID " + std::to_string(messageSentId));

    // Subscribe to message received events
    unified_event::logTrace("DHT.Node", "TRACE: Subscribing to MessageReceived events");
    int messageReceivedId = m_eventBus->subscribe(
        unified_event::EventType::MessageReceived,
        [](const std::shared_ptr<unified_event::Event>&) {
            unified_event::logTrace("DHT.Node", "TRACE: MessageReceived event received");
            // No special handling needed, just let the logging processor handle it
        });
    m_eventSubscriptionIds.push_back(messageReceivedId);
    unified_event::logTrace("DHT.Node", "TRACE: Subscribed to MessageReceived events with ID " + std::to_string(messageReceivedId));

    unified_event::logDebug("DHT.Node", "Subscribed to " + std::to_string(m_eventSubscriptionIds.size()) + " event types");
    unified_event::logTrace("DHT.Node", "TRACE: subscribeToEvents() exit");
}

void DHTNode::handleNodeDiscoveredEvent(const std::shared_ptr<unified_event::Event>& /*event*/) {
    unified_event::logTrace("DHT.Node", "TRACE: handleNodeDiscoveredEvent() entry");
    //TODO: Handle node discovered event
    unified_event::logTrace("DHT.Node", "TRACE: handleNodeDiscoveredEvent() exit");
}

void DHTNode::handlePeerDiscoveredEvent(const std::shared_ptr<unified_event::Event>& event) {
    unified_event::logTrace("DHT.Node", "TRACE: handlePeerDiscoveredEvent() entry");
    auto infoHashOpt = event->getProperty<std::string>("infoHash");
    auto endpointOpt = event->getProperty<std::string>("endpoint");

    if (infoHashOpt && endpointOpt) {
        unified_event::logTrace("DHT.Node", "TRACE: Peer discovered with info hash: " + *infoHashOpt + ", endpoint: " + *endpointOpt);
        unified_event::logDebug("DHT.Node", "Peer Discovered - Info Hash: " + *infoHashOpt + ", endpoint: " + *endpointOpt);
    } else {
        unified_event::logTrace("DHT.Node", "TRACE: Peer discovered event missing info hash or endpoint");
    }
    unified_event::logTrace("DHT.Node", "TRACE: handlePeerDiscoveredEvent() exit");
}

void DHTNode::handleSystemErrorEvent(const std::shared_ptr<unified_event::Event>& event) {
    unified_event::logTrace("DHT.Node", "TRACE: handleSystemErrorEvent() entry");
    auto messageOpt = event->getProperty<std::string>("message");
    auto source = event->getSource();

    if (messageOpt) {
        unified_event::logTrace("DHT.Node", "TRACE: System error from " + source + ": " + *messageOpt);
        unified_event::logDebug("DHT.Node", "System Error from " + source + ": " + *messageOpt);
    } else {
        unified_event::logTrace("DHT.Node", "TRACE: System error event missing message");
    }
    unified_event::logTrace("DHT.Node", "TRACE: handleSystemErrorEvent() exit");
}

void DHTNode::saveRoutingTablePeriodically() {
    unified_event::logTrace("DHT.Node", "TRACE: saveRoutingTablePeriodically() entry");
    unified_event::logDebug("DHT.Node", "Starting Routing Table Save Thread");

    while (m_running) {
        unified_event::logTrace("DHT.Node", "TRACE: Sleeping for " + std::to_string(m_config.getRoutingTableSaveInterval()) + " minutes");
        // Sleep for the configured interval
        std::this_thread::sleep_for(std::chrono::minutes(m_config.getRoutingTableSaveInterval()));

        if (!m_running) {
            unified_event::logTrace("DHT.Node", "TRACE: Node no longer running, exiting save thread");
            break;
        }

        // Save the routing table
        if (m_routingTable) {
            unified_event::logTrace("DHT.Node", "TRACE: Saving routing table");
            unified_event::logDebug("DHT.Node", "Saving Routing Table");

            std::string filePath = m_config.getRoutingTablePath();
            unified_event::logTrace("DHT.Node", "TRACE: Saving to file: " + filePath);
            bool success = m_routingTable->saveToFile(filePath);

            if (success) {
                unified_event::logTrace("DHT.Node", "TRACE: Routing table saved successfully");
                unified_event::logDebug("DHT.Node", "Routing Table Saved to " + filePath);
            } else {
                unified_event::logTrace("DHT.Node", "TRACE: Failed to save routing table");
                unified_event::logError("DHT.Node", "Failed to save routing table to " + filePath);
            }
        } else {
            unified_event::logTrace("DHT.Node", "TRACE: No routing table available to save");
        }
    }

    unified_event::logDebug("DHT.Node", "Routing Table Save Thread Ended");
    unified_event::logTrace("DHT.Node", "TRACE: saveRoutingTablePeriodically() exit");
}

std::shared_ptr<Crawler> DHTNode::getCrawler() const {
    unified_event::logTrace("DHT.Node", "TRACE: getCrawler() called");
    unified_event::logTrace("DHT.Node", "TRACE: getCrawler() returning crawler instance");
    return m_crawler;
}

std::shared_ptr<RoutingManager> DHTNode::getRoutingManager() const {
    unified_event::logTrace("DHT.Node", "TRACE: getRoutingManager() called");
    unified_event::logTrace("DHT.Node", "TRACE: getRoutingManager() returning routing manager instance");
    return m_routingManager;
}

std::shared_ptr<PeerStorage> DHTNode::getPeerStorage() const {
    unified_event::logTrace("DHT.Node", "TRACE: getPeerStorage() called");
    unified_event::logTrace("DHT.Node", "TRACE: getPeerStorage() returning peer storage instance");
    return m_peerStorage;
}

}  // namespace dht_hunter::dht
