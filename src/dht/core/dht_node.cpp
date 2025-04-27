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

namespace dht_hunter::dht {

DHTNode::DHTNode(const DHTConfig& config) : m_nodeID(generateRandomNodeID()), m_config(config), m_running(false) {
    // Create event for node initialization
    auto initEvent = std::make_shared<unified_event::CustomEvent>("DHT.Node", "Initializing", unified_event::EventSeverity::Debug);
    initEvent->setProperty("nodeID", nodeIDToString(m_nodeID));
    initEvent->setProperty("port", config.getPort());
    unified_event::EventBus::getInstance()->publish(initEvent);
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

    // Log DHT node creation
    auto createdEvent = std::make_shared<unified_event::CustomEvent>("DHT.Node", "Created", unified_event::EventSeverity::Debug);
    createdEvent->setProperty("nodeID", nodeIDToString(m_nodeID));
    createdEvent->setProperty("port", config.getPort());
    m_eventBus->publish(createdEvent);
}

DHTNode::~DHTNode() {
    stop();
}

bool DHTNode::start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running) {
        return true;
    }

    // Start the components
    // Start socket manager
    auto socketStartEvent = std::make_shared<unified_event::CustomEvent>("DHT.Node", "Starting Socket Manager", unified_event::EventSeverity::Debug);
    m_eventBus->publish(socketStartEvent);

    if (!m_socketManager->start([this](const uint8_t* data, size_t size, const network::EndPoint& sender) {
            if (m_messageHandler) {
                m_messageHandler->handleRawMessage(data, size, sender);
            }
        })) {
        auto failEvent = std::make_shared<unified_event::SystemErrorEvent>("DHT.Node", "Failed to start socket manager");
        m_eventBus->publish(failEvent);
        return false;
    }

    // Start message sender
    auto senderStartEvent = std::make_shared<unified_event::CustomEvent>("DHT.Node", "Starting Message Sender", unified_event::EventSeverity::Debug);
    m_eventBus->publish(senderStartEvent);

    if (!m_messageSender->start()) {
        auto failEvent = std::make_shared<unified_event::SystemErrorEvent>("DHT.Node", "Failed to start message sender");
        m_eventBus->publish(failEvent);
        m_socketManager->stop();
        return false;
    }

    // Start token manager
    auto tokenStartEvent = std::make_shared<unified_event::CustomEvent>("DHT.Node", "Starting Token Manager", unified_event::EventSeverity::Debug);
    m_eventBus->publish(tokenStartEvent);

    if (!m_tokenManager->start()) {
        auto failEvent = std::make_shared<unified_event::SystemErrorEvent>("DHT.Node", "Failed to start token manager");
        m_eventBus->publish(failEvent);
        m_messageSender->stop();
        m_socketManager->stop();
        return false;
    }

    // Start peer storage
    auto storageStartEvent = std::make_shared<unified_event::CustomEvent>("DHT.Node", "Starting Peer Storage", unified_event::EventSeverity::Debug);
    m_eventBus->publish(storageStartEvent);

    if (!m_peerStorage->start()) {
        auto failEvent = std::make_shared<unified_event::SystemErrorEvent>("DHT.Node", "Failed to start peer storage");
        m_eventBus->publish(failEvent);
        m_tokenManager->stop();
        m_messageSender->stop();
        m_socketManager->stop();
        return false;
    }

    // Start transaction manager
    auto transStartEvent = std::make_shared<unified_event::CustomEvent>("DHT.Node", "Starting Transaction Manager", unified_event::EventSeverity::Debug);
    m_eventBus->publish(transStartEvent);

    if (!m_transactionManager->start()) {
        auto failEvent = std::make_shared<unified_event::SystemErrorEvent>("DHT.Node", "Failed to start transaction manager");
        m_eventBus->publish(failEvent);
        m_peerStorage->stop();
        m_tokenManager->stop();
        m_messageSender->stop();
        m_socketManager->stop();
        return false;
    }

    // Start routing manager
    auto routingStartEvent = std::make_shared<unified_event::CustomEvent>("DHT.Node", "Starting Routing Manager", unified_event::EventSeverity::Debug);
    m_eventBus->publish(routingStartEvent);

    if (!m_routingManager->start()) {
        auto failEvent = std::make_shared<unified_event::SystemErrorEvent>("DHT.Node", "Failed to start routing manager");
        m_eventBus->publish(failEvent);
        m_transactionManager->stop();
        m_peerStorage->stop();
        m_tokenManager->stop();
        m_messageSender->stop();
        m_socketManager->stop();
        return false;
    }

    // Initialize DHT extensions
    if (m_mainlineDHT && !m_mainlineDHT->initialize()) {
        m_mainlineDHT.reset();
    }

    if (m_kademliaDHT && !m_kademliaDHT->initialize()) {
        m_kademliaDHT.reset();
    }

    if (m_azureusDHT && !m_azureusDHT->initialize()) {
        m_azureusDHT.reset();
    }

    m_running = true;

    // Subscribe to events
    subscribeToEvents();

    // Start the statistics service
    if (m_statisticsService && !m_statisticsService->start()) {
        // Continue anyway, this is not critical
    }

    // Bootstrap the node
    m_bootstrapper->bootstrap([this](const bool success) {
        if (success) {

            // Publish a system started event
            auto bootstrapEvent = std::make_shared<unified_event::SystemStartedEvent>("DHT.Node.Bootstrap");
            m_eventBus->publish(bootstrapEvent);
        } else {
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
        return;
    }

    // Stop the DHT extensions
    if (m_azureusDHT) {
        m_azureusDHT->shutdown();
    }
    if (m_kademliaDHT) {
        m_kademliaDHT->shutdown();
    }
    if (m_mainlineDHT) {
        m_mainlineDHT->shutdown();
    }

    // Stop the components in reverse order
    m_routingManager->stop();
    m_transactionManager->stop();
    m_peerStorage->stop();
    m_tokenManager->stop();
    m_messageSender->stop();
    m_socketManager->stop();

    // Unsubscribe from events
    for (int subscriptionId : m_eventSubscriptionIds) {
        m_eventBus->unsubscribe(subscriptionId);
    }
    m_eventSubscriptionIds.clear();

    // Stop the statistics service
    if (m_statisticsService) {
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
    if (!m_running) {
        if (callback) {
            callback({});
        }
        return;
    }

    if (!m_nodeLookup) {
        if (callback) {
            callback({});
        }
        return;
    }

    m_nodeLookup->lookup(targetID, callback);
}

void DHTNode::getPeers(const InfoHash& infoHash, const std::function<void(const std::vector<network::EndPoint>&)>& callback) const {
    if (!m_running) {
        if (callback) {
            callback({});
        }
        return;
    }

    if (!m_peerLookup) {
        if (callback) {
            callback({});
        }
        return;
    }

    m_peerLookup->lookup(infoHash, callback);
}

void DHTNode::announcePeer(const InfoHash& infoHash, const uint16_t port, const std::function<void(bool)>& callback) const {
    if (!m_running) {
        if (callback) {
            callback(false);
        }
        return;
    }

    if (!m_peerLookup) {
        if (callback) {
            callback(false);
        }
        return;
    }

    m_peerLookup->announce(infoHash, port, callback);
}

void DHTNode::ping(const std::shared_ptr<Node>& node, const std::function<void(bool)>& callback) {
    if (!m_running) {
        if (callback) {
            callback(false);
        }
        return;
    }

    if (!m_messageSender || !m_transactionManager) {
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
        [callback](const std::shared_ptr<ResponseMessage>& /*response*/, const network::EndPoint& /*sender*/) {
            if (callback) {
                callback(true);
            }
        },
        [callback](const std::shared_ptr<ErrorMessage>& /*error*/, const network::EndPoint& /*sender*/) {
            if (callback) {
                callback(false);
            }
        },
        [callback]() {
            if (callback) {
                callback(false);
            }
        });

    if (!transactionID.empty()) {
        m_messageSender->sendQuery(query, node->getEndpoint());
    } else {
        if (callback) {
            callback(false);
        }
    }
}

}  // namespace dht_hunter::dht
