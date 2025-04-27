#include "dht_hunter/dht/core/dht_node.hpp"

#include "dht_hunter/dht/bootstrap/bootstrapper.hpp"
#include "dht_hunter/dht/extensions/azureus_dht.hpp"
#include "dht_hunter/dht/extensions/kademlia_dht.hpp"
#include "dht_hunter/dht/extensions/mainline_dht.hpp"
#include "dht_hunter/dht/network/message_handler.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include "dht_hunter/dht/network/socket_manager.hpp"
#include "dht_hunter/dht/routing/node_lookup.hpp"
#include "dht_hunter/dht/routing/peer_lookup.hpp"
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
    // Log node initialization
    unified_event::logDebug("DHT.Node", "Initializing node with ID: " + nodeIDToString(m_nodeID));
    // Create the components
    m_routingTable = RoutingTable::getInstance(m_nodeID, config.getKBucketSize());
    m_socketManager = SocketManager::getInstance(config);
    m_messageSender = MessageSender::getInstance(config, m_socketManager);
    m_tokenManager = TokenManager::getInstance(config);
    m_peerStorage = PeerStorage::getInstance(config);
    m_transactionManager = TransactionManager::getInstance(config);
    m_routingManager = RoutingManager::getInstance(config, m_nodeID, m_routingTable, m_transactionManager, m_messageSender);
    m_nodeLookup = NodeLookup::getInstance(config, m_nodeID, m_routingTable, m_transactionManager, m_messageSender);
    m_peerLookup = PeerLookup::getInstance(config, m_nodeID, m_routingTable, m_transactionManager, m_messageSender, m_tokenManager, m_peerStorage);
    m_bootstrapper = Bootstrapper::getInstance(config, m_nodeID, m_routingManager, m_nodeLookup);
    m_messageHandler = MessageHandler::getInstance(config, m_nodeID, m_messageSender, m_routingTable, m_tokenManager, m_peerStorage, m_transactionManager);
    m_btMessageHandler = bittorrent::BTMessageHandler::getInstance(m_routingManager);

    // Get the event bus
    m_eventBus = unified_event::EventBus::getInstance();

    // Get the statistics service
    m_statisticsService = services::StatisticsService::getInstance();

    // Create DHT extensions
    m_mainlineDHT = std::make_shared<extensions::MainlineDHT>(config, m_nodeID, m_routingTable);
    m_kademliaDHT = std::make_shared<extensions::KademliaDHT>(config, m_nodeID, m_routingTable);
    m_azureusDHT = std::make_shared<extensions::AzureusDHT>(config, m_nodeID, m_routingTable);

    // Create the crawler
    m_crawler = Crawler::getInstance(config, m_nodeID, m_routingManager, m_nodeLookup, m_peerLookup, m_transactionManager, m_messageSender, m_peerStorage);

    // Log DHT node creation
    unified_event::logInfo("DHT.Node", "Node initialized with ID: " + nodeIDToString(m_nodeID));
}

DHTNode::~DHTNode() {
    stop();
}

bool DHTNode::start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running) {
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
    unified_event::logDebug("DHT.Node", "Initializing Mainline DHT Extension");

    if (m_mainlineDHT && !m_mainlineDHT->initialize()) {
        unified_event::logError("DHT.Node", "Failed to initialize Mainline DHT extension");
        m_mainlineDHT.reset();
    } else if (m_mainlineDHT) {
        unified_event::logDebug("DHT.Node", "Mainline DHT Extension Initialized");
    }

    unified_event::logDebug("DHT.Node", "Initializing Kademlia DHT Extension");

    if (m_kademliaDHT && !m_kademliaDHT->initialize()) {
        unified_event::logError("DHT.Node", "Failed to initialize Kademlia DHT extension");
        m_kademliaDHT.reset();
    } else if (m_kademliaDHT) {
        unified_event::logDebug("DHT.Node", "Kademlia DHT Extension Initialized");
    }

    unified_event::logDebug("DHT.Node", "Initializing Azureus DHT Extension");

    if (m_azureusDHT && !m_azureusDHT->initialize()) {
        unified_event::logError("DHT.Node", "Failed to initialize Azureus DHT extension");
        m_azureusDHT.reset();
    } else if (m_azureusDHT) {
        unified_event::logDebug("DHT.Node", "Azureus DHT Extension Initialized");
    }

    m_running = true;

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
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running) {
        unified_event::logDebug("DHT.Node", "Stop called while not running");
        return;
    }

    unified_event::logDebug("DHT.Node", "Stopping DHT Node");

    // Stop the DHT extensions
    unified_event::logDebug("DHT.Node", "Stopping DHT Extensions");

    if (m_azureusDHT) {
        unified_event::logDebug("DHT.Node", "Stopping Azureus DHT");
        m_azureusDHT->shutdown();
    }
    if (m_kademliaDHT) {
        unified_event::logDebug("DHT.Node", "Stopping Kademlia DHT");
        m_kademliaDHT->shutdown();
    }
    if (m_mainlineDHT) {
        unified_event::logDebug("DHT.Node", "Stopping Mainline DHT");
        m_mainlineDHT->shutdown();
    }

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

    m_running = false;
}

bool DHTNode::isRunning() const {
    return m_running;
}

const NodeID& DHTNode::getNodeID() const {
    return m_nodeID;
}

uint16_t DHTNode::getPort() const {
    return m_config.getPort();
}

std::string DHTNode::getStatistics() const {
    if (m_statisticsService) {
        return m_statisticsService->getStatisticsAsJson();
    }
    return "{}";
}

bool DHTNode::handlePortMessage(const network::NetworkAddress& peerAddress, const uint8_t* data, size_t length) const {
    if (!m_running || !m_btMessageHandler) {
        return false;
    }

    return m_btMessageHandler->handlePortMessage(peerAddress, data, length);
}

bool DHTNode::handlePortMessage(const network::NetworkAddress& peerAddress, uint16_t port) const {
    if (!m_running || !m_btMessageHandler) {
        return false;
    }

    return m_btMessageHandler->handlePortMessage(peerAddress, port);
}

std::vector<uint8_t> DHTNode::createPortMessage() const {
    uint16_t port = getPort();
    return bittorrent::BTMessageHandler::createPortMessage(port);
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
            }
        },
        [node, callback](const std::shared_ptr<ErrorMessage>& /*error*/, const network::EndPoint& /*sender*/) {
            unified_event::logDebug("DHT.Node", "Ping Error for node ID: " + nodeIDToString(node->getID()) + ", endpoint: " + node->getEndpoint().toString());

            if (callback) {
                callback(false);
            }
        },
        [node, callback]() {
            unified_event::logDebug("DHT.Node", "Ping Timeout for node ID: " + nodeIDToString(node->getID()) + ", endpoint: " + node->getEndpoint().toString());

            if (callback) {
                callback(false);
            }
        });

    if (!transactionID.empty()) {
        unified_event::logDebug("DHT.Node", "Sending Ping to node ID: " + nodeIDToString(node->getID()) + ", endpoint: " + node->getEndpoint().toString() + ", transaction ID: " + transactionID);

        m_messageSender->sendQuery(query, node->getEndpoint());
    } else {
        unified_event::logError("DHT.Node", "Failed to create transaction for node ID: " + nodeIDToString(node->getID()) + ", endpoint: " + node->getEndpoint().toString());

        if (callback) {
            callback(false);
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
    }
}

void DHTNode::handleSystemErrorEvent(const std::shared_ptr<unified_event::Event>& event) {
    auto messageOpt = event->getProperty<std::string>("message");
    auto source = event->getSource();

    if (messageOpt) {
        unified_event::logDebug("DHT.Node", "System Error from " + source + ": " + *messageOpt);
    }
}

void DHTNode::saveRoutingTablePeriodically() {
    unified_event::logDebug("DHT.Node", "Starting Routing Table Save Thread");

    while (m_running) {
        // Sleep for the configured interval
        std::this_thread::sleep_for(std::chrono::minutes(m_config.getRoutingTableSaveInterval()));

        if (!m_running) {
            break;
        }

        // Save the routing table
        if (m_routingTable) {
            unified_event::logDebug("DHT.Node", "Saving Routing Table");

            std::string filePath = m_config.getRoutingTablePath();
            bool success = m_routingTable->saveToFile(filePath);

            if (success) {
                unified_event::logDebug("DHT.Node", "Routing Table Saved to " + filePath);
            } else {
                unified_event::logError("DHT.Node", "Failed to save routing table to " + filePath);
            }
        }
    }

    unified_event::logDebug("DHT.Node", "Routing Table Save Thread Ended");
}

std::shared_ptr<Crawler> DHTNode::getCrawler() const {
    return m_crawler;
}

}  // namespace dht_hunter::dht
