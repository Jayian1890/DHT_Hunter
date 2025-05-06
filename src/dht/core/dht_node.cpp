#include "dht_hunter/dht/core/dht_node.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"

#include "dht_hunter/dht/bootstrap/bootstrapper.hpp"
#include "dht_hunter/dht/extensions/factory/extension_factory.hpp"
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

DHTNode::DHTNode(const DHTConfig& config, const NodeID& nodeID) : m_nodeID(nodeID), m_config(config), m_running(false) {

    // Log node initialization
    unified_event::logDebug("DHT.Node", "Initializing node with ID: " + nodeIDToString(m_nodeID));

    // Create the components in dependency order

    // 1. Core services that don't depend on other components
    m_eventBus = unified_event::EventBus::getInstance();

    m_statisticsService = services::StatisticsService::getInstance();

    m_extensionFactory = extensions::ExtensionFactory::getInstance();

    // 2. Basic network and storage components
    m_socketManager = SocketManager::getInstance(config);

    m_tokenManager = TokenManager::getInstance(config);

    m_peerStorage = PeerStorage::getInstance(config);

    m_routingTable = RoutingTable::getInstance(m_nodeID, config.getKBucketSize());

    // 3. Components that depend on basic components
    m_messageSender = MessageSender::getInstance(config, m_socketManager);

    m_transactionManager = TransactionManager::getInstance(config);

    // 4. Higher-level components that depend on multiple other components
    m_routingManager = RoutingManager::getInstance(config, m_nodeID, m_routingTable, m_transactionManager, m_messageSender);

    m_nodeLookup = NodeLookup::getInstance(config, m_nodeID, m_routingTable, m_transactionManager, m_messageSender);

    m_peerLookup = PeerLookup::getInstance(config, m_nodeID, m_routingTable, m_transactionManager, m_messageSender, m_tokenManager, m_peerStorage);

    // 5. Components that depend on higher-level components
    m_messageHandler = MessageHandler::getInstance(config, m_nodeID, m_messageSender, m_routingTable, m_tokenManager, m_peerStorage, m_transactionManager);

    m_btMessageHandler = bittorrent::BTMessageHandler::getInstance(m_routingManager);

    m_bootstrapper = Bootstrapper::getInstance(config, m_nodeID, m_routingManager, m_nodeLookup);

    // Create DHT extensions
    std::vector<std::string> extensionNames = {"mainline", "kademlia", "azureus"};
    for (const auto& name : extensionNames) {
        auto extension = m_extensionFactory->createExtension(name, config, m_nodeID, m_routingTable);
        if (extension) {
            m_extensions[name] = extension;
        }
    }

    // Create the crawler
    m_crawler = Crawler::getInstance(config, m_nodeID, m_routingManager, m_nodeLookup, m_peerLookup, m_transactionManager, m_messageSender, m_peerStorage);


    // Log DHT node creation
    unified_event::logInfo("DHT.Node", "Node initialized with ID: " + nodeIDToString(m_nodeID));
}

DHTNode::~DHTNode() {
    stop();
}

bool DHTNode::start() {

    // Check if already running
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        // Already running
        return true;
    }

    // Start socket manager
    unified_event::logDebug("DHT.Node", "Starting Socket Manager");

    if (!m_socketManager->start([this](const uint8_t* data, size_t size, const network::EndPoint& sender) {
            if (m_messageHandler) {
                m_messageHandler->handleRawMessage(data, size, sender);
            }
        })) {
        unified_event::logError("DHT.Node", "Failed to start socket manager");
        return false;
    }


    // Start message sender
    unified_event::logDebug("DHT.Node", "Starting Message Sender");

    if (!m_messageSender->start()) {
        unified_event::logError("DHT.Node", "Failed to start message sender");
        m_socketManager->stop();
        return false;
    }


    // Start token manager
    unified_event::logDebug("DHT.Node", "Starting Token Manager");

    if (!m_tokenManager->start()) {
        unified_event::logError("DHT.Node", "Failed to start token manager");
        m_messageSender->stop();
        m_socketManager->stop();
        return false;
    }


    // Start peer storage
    unified_event::logDebug("DHT.Node", "Starting Peer Storage");

    if (!m_peerStorage->start()) {
        unified_event::logError("DHT.Node", "Failed to start peer storage");
        m_tokenManager->stop();
        m_messageSender->stop();
        m_socketManager->stop();
        return false;
    }


    // Start transaction manager
    unified_event::logDebug("DHT.Node", "Starting Transaction Manager");

    if (!m_transactionManager->start()) {
        unified_event::logError("DHT.Node", "Failed to start transaction manager");
        m_peerStorage->stop();
        m_tokenManager->stop();
        m_messageSender->stop();
        m_socketManager->stop();
        return false;
    }


    // Start routing manager
    unified_event::logDebug("DHT.Node", "Starting Routing Manager");

    if (!m_routingManager->start()) {
        unified_event::logError("DHT.Node", "Failed to start routing manager");
        m_transactionManager->stop();
        m_peerStorage->stop();
        m_tokenManager->stop();
        m_messageSender->stop();
        m_socketManager->stop();
        return false;
    }


    // Initialize DHT extensions
    unified_event::logDebug("DHT.Node", "Initializing DHT Extensions");

    // Make a copy of the extensions map to avoid iterator invalidation
    auto extensionsCopy = m_extensions;
    for (const auto& [name, extension] : extensionsCopy) {
        unified_event::logDebug("DHT.Node", "Initializing " + extension->getName() + " Extension");

        if (!extension->initialize()) {
            unified_event::logError("DHT.Node", "Failed to initialize " + extension->getName() + " extension");
            m_extensions.erase(name);
        } else {
            unified_event::logDebug("DHT.Node", extension->getName() + " Extension Initialized");
        }
    }

    // m_running is already set to true by the compare_exchange_strong above

    // Subscribe to events
    subscribeToEvents();

    // Start the statistics service
    unified_event::logDebug("DHT.Node", "Starting Statistics Service");

    if (m_statisticsService && !m_statisticsService->start()) {
        unified_event::logWarning("DHT.Node", "Failed to start Statistics Service");
        // Continue anyway, this is not critical
    } else if (m_statisticsService) {
        unified_event::logDebug("DHT.Node", "Statistics Service Started");
    }

    // Bootstrap the node
    unified_event::logDebug("DHT.Node", "Starting Bootstrap");

    m_bootstrapper->bootstrap([this](const bool success) {
        if (success) {
            unified_event::logDebug("DHT.Node", "Bootstrap Completed, node count: " + std::to_string(m_routingTable->getNodeCount()));

            // Publish a system started event
            auto bootstrapEvent = std::make_shared<unified_event::SystemStartedEvent>("DHT.Node.Bootstrap");
            m_eventBus->publish(bootstrapEvent);
        } else {
            unified_event::logWarning("DHT.Node", "Bootstrap Failed");
        }
    });

    // Publish a system started event
    auto startedEvent = std::make_shared<unified_event::SystemStartedEvent>("DHT.Node");
    m_eventBus->publish(startedEvent);

    return true;
}

void DHTNode::stop() {

    // Check if already stopped
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
        // Already stopped
        unified_event::logDebug("DHT.Node", "Stop called while not running");
        return;
    }

    unified_event::logDebug("DHT.Node", "Stopping DHT Node");

    // Stop the DHT extensions
    unified_event::logDebug("DHT.Node", "Stopping DHT Extensions");

    // Shutdown all extensions
    for (auto& [name, extension] : m_extensions) {
        if (extension) {
            unified_event::logDebug("DHT.Node", "Stopping " + extension->getName());
            extension->shutdown();
        }
    }
    m_extensions.clear();

    // Stop the components in reverse order
    unified_event::logDebug("DHT.Node", "Stopping Components");

    unified_event::logDebug("DHT.Node", "Stopping Routing Manager");
    m_routingManager->stop();

    unified_event::logDebug("DHT.Node", "Stopping Transaction Manager");
    m_transactionManager->stop();

    unified_event::logDebug("DHT.Node", "Stopping Peer Storage");
    m_peerStorage->stop();

    unified_event::logDebug("DHT.Node", "Stopping Token Manager");
    m_tokenManager->stop();

    unified_event::logDebug("DHT.Node", "Stopping Message Sender");
    m_messageSender->stop();

    unified_event::logDebug("DHT.Node", "Stopping Socket Manager");
    m_socketManager->stop();

    // Unsubscribe from events
    for (int subscriptionId : m_eventSubscriptionIds) {
        m_eventBus->unsubscribe(subscriptionId);
    }
    m_eventSubscriptionIds.clear();

    // Stop the statistics service
    if (m_statisticsService) {
        unified_event::logDebug("DHT.Node", "Stopping Statistics Service");
        m_statisticsService->stop();
    }

    // Publish a system stopped event
    auto stoppedEvent = std::make_shared<unified_event::SystemStoppedEvent>("DHT.Node");
    m_eventBus->publish(stoppedEvent);

    // m_running is already set to false by the compare_exchange_strong above
}

bool DHTNode::isRunning() const {
    bool running = m_running.load(std::memory_order_acquire);
    return running;
}

const NodeID& DHTNode::getNodeID() const {
    return m_nodeID;
}

uint16_t DHTNode::getPort() const {
    uint16_t port = m_config.getPort();
    return port;
}

std::string DHTNode::getStatistics() const {
    if (m_statisticsService) {
        std::string stats = m_statisticsService->getStatisticsAsString();
        return stats;
    }
    return "No statistics available";
}

bool DHTNode::handlePortMessage(const network::NetworkAddress& peerAddress, const uint8_t* data, size_t length) const {
    if (!m_running || !m_btMessageHandler) {
        return false;
    }

    bool result = m_btMessageHandler->handlePortMessage(peerAddress, data, length);
    return result;
}

bool DHTNode::handlePortMessage(const network::NetworkAddress& peerAddress, uint16_t port) const {
    if (!m_running || !m_btMessageHandler) {
        return false;
    }

    bool result = m_btMessageHandler->handlePortMessage(peerAddress, port);
    return result;
}

std::vector<uint8_t> DHTNode::createPortMessage() const {
    uint16_t port = getPort();
    std::vector<uint8_t> message = bittorrent::BTMessageHandler::createPortMessage(port);
    return message;
}

std::shared_ptr<RoutingTable> DHTNode::getRoutingTable() const {
    return m_routingTable;
}

void DHTNode::findClosestNodes(const NodeID& targetID, const std::function<void(const std::vector<std::shared_ptr<Node>>&)>& callback) const {
    unified_event::logDebug("DHT.Node", "Find Closest Nodes for target ID: " + nodeIDToString(targetID));

    if (!m_running) {
        unified_event::logError("DHT.Node", "Find Closest Nodes called while not running");
        if (callback) {
            callback({});
        }
        return;
    }

    if (!m_nodeLookup) {
        unified_event::logError("DHT.Node", "No node lookup available");
        if (callback) {
            callback({});
        }
        return;
    }

    m_nodeLookup->lookup(targetID, [targetID, callback](const std::vector<std::shared_ptr<Node>>& nodes) {
        unified_event::logDebug("DHT.Node", "Find Closest Nodes Result for target ID: " + nodeIDToString(targetID) + ", found " + std::to_string(nodes.size()) + " nodes");

        if (callback) {
            callback(nodes);
        } else {
        }
    });
}

void DHTNode::getPeers(const InfoHash& infoHash, const std::function<void(const std::vector<network::EndPoint>&)>& callback) const {
    unified_event::logDebug("DHT.Node", "Get Peers for info hash: " + infoHashToString(infoHash));

    if (!m_running) {
        unified_event::logError("DHT.Node", "Get Peers called while not running");
        if (callback) {
            callback({});
        }
        return;
    }

    if (!m_peerLookup) {
        unified_event::logError("DHT.Node", "No peer lookup available");
        if (callback) {
            callback({});
        }
        return;
    }

    m_peerLookup->lookup(infoHash, [infoHash, callback](const std::vector<network::EndPoint>& peers) {
        unified_event::logDebug("DHT.Node", "Get Peers Result for info hash: " + infoHashToString(infoHash) + ", found " + std::to_string(peers.size()) + " peers");

        if (callback) {
            callback(peers);
        } else {
        }
    });
}

void DHTNode::announcePeer(const InfoHash& infoHash, const uint16_t port, const std::function<void(bool)>& callback) const {
    unified_event::logDebug("DHT.Node", "Announce Peer for info hash: " + infoHashToString(infoHash) + ", port: " + std::to_string(port));

    if (!m_running) {
        unified_event::logError("DHT.Node", "Announce Peer called while not running");
        if (callback) {
            callback(false);
        }
        return;
    }

    if (!m_peerLookup) {
        unified_event::logError("DHT.Node", "No peer lookup available");
        if (callback) {
            callback(false);
        }
        return;
    }

    m_peerLookup->announce(infoHash, port, [infoHash, port, callback](bool success) {
        unified_event::logDebug("DHT.Node", "Announce Peer Result for info hash: " + infoHashToString(infoHash) + ", port: " + std::to_string(port) + ", success: " + (success ? "true" : "false"));

        if (callback) {
            callback(success);
        } else {
        }
    });
}

void DHTNode::ping(const std::shared_ptr<Node>& node, const std::function<void(bool)>& callback) {
    unified_event::logDebug("DHT.Node", "Ping Node with ID: " + nodeIDToString(node->getID()) + ", endpoint: " + node->getEndpoint().toString());

    if (!m_running) {
        unified_event::logError("DHT.Node", "Ping called while not running");
        if (callback) {
            callback(false);
        }
        return;
    }

    if (!m_messageSender || !m_transactionManager) {
        unified_event::logError("DHT.Node", "No message sender or transaction manager available");
        if (callback) {
            callback(false);
        }
        return;
    }

    // Create a ping query
    auto query = std::make_shared<PingQuery>("", m_nodeID);

    // Send the query
    std::string transactionID = m_transactionManager->createTransaction(
        query, node->getEndpoint(),
        [node, callback](const std::shared_ptr<ResponseMessage>& /*response*/, const network::EndPoint& /*sender*/) {
            unified_event::logDebug("DHT.Node", "Ping Success for node ID: " + nodeIDToString(node->getID()) + ", endpoint: " + node->getEndpoint().toString());

            if (callback) {
                callback(true);
            } else {
            }
        },
        [node, callback](const std::shared_ptr<ErrorMessage>& /*error*/, const network::EndPoint& /*sender*/) {
            unified_event::logDebug("DHT.Node", "Ping Error for node ID: " + nodeIDToString(node->getID()) + ", endpoint: " + node->getEndpoint().toString());

            if (callback) {
                callback(false);
            } else {
            }
        },
        [node, callback]() {
            unified_event::logDebug("DHT.Node", "Ping Timeout for node ID: " + nodeIDToString(node->getID()) + ", endpoint: " + node->getEndpoint().toString());

            if (callback) {
                callback(false);
            } else {
            }
        });

    if (!transactionID.empty()) {
        unified_event::logDebug("DHT.Node", "Sending Ping to node ID: " + nodeIDToString(node->getID()) + ", endpoint: " + node->getEndpoint().toString() + ", transaction ID: " + transactionID);

        m_messageSender->sendQuery(query, node->getEndpoint());
    } else {
        unified_event::logError("DHT.Node", "Failed to create transaction for node ID: " + nodeIDToString(node->getID()) + ", endpoint: " + node->getEndpoint().toString());

        if (callback) {
            callback(false);
        } else {
        }
    }
}

void DHTNode::subscribeToEvents() {
    unified_event::logDebug("DHT.Node", "Subscribing to Events");

    // Subscribe to node discovered events
    int nodeDiscoveredId = m_eventBus->subscribe(
        unified_event::EventType::NodeDiscovered,
        [this](const std::shared_ptr<unified_event::Event>& event) {
            this->handleNodeDiscoveredEvent(event);
        });
    m_eventSubscriptionIds.push_back(nodeDiscoveredId);

    // Subscribe to peer discovered events
    int peerDiscoveredId = m_eventBus->subscribe(
        unified_event::EventType::PeerDiscovered,
        [this](const std::shared_ptr<unified_event::Event>& event) {
            this->handlePeerDiscoveredEvent(event);
        });
    m_eventSubscriptionIds.push_back(peerDiscoveredId);

    // Subscribe to system error events
    int systemErrorId = m_eventBus->subscribe(
        unified_event::EventType::SystemError,
        [this](const std::shared_ptr<unified_event::Event>& event) {
            this->handleSystemErrorEvent(event);
        });
    m_eventSubscriptionIds.push_back(systemErrorId);

    // Subscribe to message sent events
    int messageSentId = m_eventBus->subscribe(
        unified_event::EventType::MessageSent,
        [](const std::shared_ptr<unified_event::Event>&) {
            // No special handling needed, just let the logging processor handle it
        });
    m_eventSubscriptionIds.push_back(messageSentId);

    // Subscribe to message received events
    int messageReceivedId = m_eventBus->subscribe(
        unified_event::EventType::MessageReceived,
        [](const std::shared_ptr<unified_event::Event>&) {
            // No special handling needed, just let the logging processor handle it
        });
    m_eventSubscriptionIds.push_back(messageReceivedId);

    unified_event::logDebug("DHT.Node", "Subscribed to " + std::to_string(m_eventSubscriptionIds.size()) + " event types");
}

void DHTNode::handleNodeDiscoveredEvent(const std::shared_ptr<unified_event::Event>& /*event*/) {
    //TODO: Handle node discovered event
}

void DHTNode::handlePeerDiscoveredEvent(const std::shared_ptr<unified_event::Event>& event) {
    auto infoHashOpt = event->getProperty<std::string>("infoHash");
    auto endpointOpt = event->getProperty<std::string>("endpoint");

    if (infoHashOpt && endpointOpt) {
        unified_event::logDebug("DHT.Node", "Peer Discovered - Info Hash: " + *infoHashOpt + ", endpoint: " + *endpointOpt);
    } else {
    }
}

void DHTNode::handleSystemErrorEvent(const std::shared_ptr<unified_event::Event>& event) {
    auto messageOpt = event->getProperty<std::string>("message");
    auto source = event->getSource();

    if (messageOpt) {
        unified_event::logDebug("DHT.Node", "System Error from " + source + ": " + *messageOpt);
    } else {
    }
}

// Routing table saving method has been removed
// This operation is now handled by the PersistenceManager

std::shared_ptr<Crawler> DHTNode::getCrawler() const {
    return m_crawler;
}

std::shared_ptr<RoutingManager> DHTNode::getRoutingManager() const {
    return m_routingManager;
}

std::shared_ptr<PeerStorage> DHTNode::getPeerStorage() const {
    return m_peerStorage;
}

bool DHTNode::sendQueryToNode(const NodeID& nodeId, std::shared_ptr<QueryMessage> query,
                             std::function<void(std::shared_ptr<ResponseMessage>, bool)> callback) {

    if (!m_running) {
        unified_event::logError("DHT.Node", "Send query to node called while not running");
        return false;
    }

    // Find the node in the routing table
    auto nodes = m_routingTable->getClosestNodes(nodeId, 1);
    std::shared_ptr<Node> node;
    if (!nodes.empty()) {
        node = nodes[0];
    }
    if (!node) {
        unified_event::logWarning("DHT.Node", "Node not found in routing table: " + nodeIDToString(nodeId));
        return false;
    }

    // Get the endpoint
    auto endpoint = node->getEndpoint();

    // Create a transaction
    auto transactionManager = transactions::TransactionManager::getInstance(m_config, m_nodeID);

    // Create a response callback
    auto responseCallback = [callback](std::shared_ptr<ResponseMessage> response, const EndPoint& sender) {
        callback(response, true);
    };

    // Create an error callback
    auto errorCallback = [callback](std::shared_ptr<ErrorMessage> /*error*/, const EndPoint& sender) {
        callback(nullptr, false);
    };

    // Create a timeout callback
    auto timeoutCallback = [callback]() {
        callback(nullptr, false);
    };

    // Create the transaction
    transactionManager->createTransaction(query, endpoint, responseCallback, errorCallback, timeoutCallback);

    // Send the query
    auto messageSender = MessageSender::getInstance(m_config, nullptr);
    bool sent = messageSender->sendQuery(query, endpoint);

    if (sent) {
        unified_event::logDebug("DHT.Node", "Sent query to node: " + nodeIDToString(nodeId));
    } else {
        unified_event::logWarning("DHT.Node", "Failed to send query to node: " + nodeIDToString(nodeId));
    }

    return sent;
}

void DHTNode::findNodesWithMetadata(const InfoHash& infoHash,
                                   std::function<void(const std::vector<std::shared_ptr<Node>>&)> callback) {

    if (!m_running) {
        unified_event::logError("DHT.Node", "Find nodes with metadata called while not running");
        if (callback) {
            callback({});
        }
        return;
    }

    // Convert the info hash to a node ID
    NodeID targetID;
    std::array<uint8_t, 20> bytes;
    std::copy(infoHash.begin(), infoHash.end(), bytes.begin());
    targetID = NodeID(bytes);

    // First, check if we have any peers for this info hash
    auto peers = m_peerStorage->getPeers(infoHash);
    if (!peers.empty()) {

        // If we have peers, try to find the nodes that announced them
        std::vector<std::shared_ptr<Node>> nodes;
        for (const auto& peer : peers) {
            // Find a node with this endpoint
            std::shared_ptr<Node> node;
            auto allNodes = m_routingTable->getAllNodes();
            for (const auto& n : allNodes) {
                if (n->getEndpoint() == peer) {
                    node = n;
                    break;
                }
            }
            if (node) {
                nodes.push_back(node);
            }
        }

        if (!nodes.empty()) {
            unified_event::logDebug("DHT.Node", "Found " + std::to_string(nodes.size()) + " nodes with peers for info hash: " + infoHashToString(infoHash));
            callback(nodes);
            return;
        }
    }


    // If we don't have any peers, use the closest nodes
    findClosestNodes(targetID, [infoHash, callback](const std::vector<std::shared_ptr<Node>>& nodes) {
        unified_event::logDebug("DHT.Node", "Found " + std::to_string(nodes.size()) + " closest nodes for info hash: " + infoHashToString(infoHash));
        callback(nodes);
    });
}

}  // namespace dht_hunter::dht
