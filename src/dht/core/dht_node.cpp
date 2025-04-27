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

#include <dht_hunter/unified_event/adapters/dht_event_adapter.hpp>

namespace dht_hunter::dht {

DHTNode::DHTNode(const DHTConfig& config)
    : m_nodeID(generateRandomNodeID()),
      m_config(config),
      m_running(false),
      m_logger(event::Logger::forComponent("DHT.Node")) {

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
}

DHTNode::~DHTNode() {
    stop();
}

bool DHTNode::start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running) {
        m_logger.warning("DHT node already running");
        return true;
    }

    // Start the components
    if (!m_socketManager->start([this](const uint8_t* data, size_t size, const network::EndPoint& sender) {
        if (m_messageHandler) {
            m_messageHandler->handleRawMessage(data, size, sender);
        }
    })) {
        m_logger.error("Failed to start socket manager");
        return false;
    }

    if (!m_messageSender->start()) {
        m_logger.error("Failed to start message sender");
        m_socketManager->stop();
        return false;
    }

    if (!m_tokenManager->start()) {
        m_logger.error("Failed to start token manager");
        m_messageSender->stop();
        m_socketManager->stop();
        return false;
    }

    if (!m_peerStorage->start()) {
        m_logger.error("Failed to start peer storage");
        m_tokenManager->stop();
        m_messageSender->stop();
        m_socketManager->stop();
        return false;
    }

    if (!m_transactionManager->start()) {
        m_logger.error("Failed to start transaction manager");
        m_peerStorage->stop();
        m_tokenManager->stop();
        m_messageSender->stop();
        m_socketManager->stop();
        return false;
    }

    if (!m_routingManager->start()) {
        m_logger.error("Failed to start routing manager");
        m_transactionManager->stop();
        m_peerStorage->stop();
        m_tokenManager->stop();
        m_messageSender->stop();
        m_socketManager->stop();
        return false;
    }

    // Initialize DHT extensions
    if (m_mainlineDHT && !m_mainlineDHT->initialize()) {
        m_logger.error("Failed to initialize Mainline DHT extension, continuing without it");
        m_mainlineDHT.reset();
    }

    if (m_kademliaDHT && !m_kademliaDHT->initialize()) {
        m_logger.error("Failed to initialize Kademlia DHT extension, continuing without it");
        m_kademliaDHT.reset();
    }

    if (m_azureusDHT && !m_azureusDHT->initialize()) {
        m_logger.error("Failed to initialize Azureus DHT extension, continuing without it");
        m_azureusDHT.reset();
    }

    m_running = true;

    // Subscribe to events
    subscribeToEvents();

    // Start the statistics service
    if (m_statisticsService && !m_statisticsService->start()) {
        m_logger.error("Failed to start statistics service");
        // Continue anyway, this is not critical
    }

    // Bootstrap the node
    m_bootstrapper->bootstrap([this](const bool success) {
        if (success) {
            m_logger.info("DHT node bootstrapped successfully");

            // Publish a system started event
            auto bootstrapEvent = std::make_shared<unified_event::SystemStartedEvent>("DHT.Node.Bootstrap");
            m_eventBus->publish(bootstrapEvent);
        } else {
            m_logger.warning("DHT node bootstrap failed");
        }
    });

    m_logger.info("DHT node created with ID: {}", nodeIDToString(m_nodeID));

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

    m_logger.info("Stopping DHT node");

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

    m_logger.info("DHT node stopped");
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

bool DHTNode::handlePortMessage(const network::NetworkAddress& peerAddress, const uint8_t* data, size_t length) {
    if (!m_running || !m_btMessageHandler) {
        return false;
    }

    return m_btMessageHandler->handlePortMessage(peerAddress, data, length);
}

bool DHTNode::handlePortMessage(const network::NetworkAddress& peerAddress, uint16_t port) {
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

void DHTNode::findClosestNodes(const NodeID& targetID,
    const std::function<void(const std::vector<std::shared_ptr<Node>> &)>
        &callback)
    const {
    if (!m_running) {
        m_logger.error("DHT node not running");
        if (callback) {
            callback({});
        }
        return;
    }

    if (!m_nodeLookup) {
        m_logger.error("No node lookup available");
        if (callback) {
            callback({});
        }
        return;
    }

    m_nodeLookup->lookup(targetID, callback);
}

void DHTNode::getPeers(const InfoHash& infoHash,
    const std::function<void(const std::vector<network::EndPoint> &)> &callback)
    const {
    if (!m_running) {
        m_logger.error("DHT node not running");
        if (callback) {
            callback({});
        }
        return;
    }

    if (!m_peerLookup) {
        m_logger.error("No peer lookup available");
        if (callback) {
            callback({});
        }
        return;
    }

    m_peerLookup->lookup(infoHash, callback);
}

void DHTNode::announcePeer(const InfoHash& infoHash, const uint16_t port,
                           const std::function<void(bool)> &callback) const {
    if (!m_running) {
        m_logger.error("DHT node not running");
        if (callback) {
            callback(false);
        }
        return;
    }

    if (!m_peerLookup) {
        m_logger.error("No peer lookup available");
        if (callback) {
            callback(false);
        }
        return;
    }

    m_peerLookup->announce(infoHash, port, callback);
}

void DHTNode::ping(const std::shared_ptr<Node>& node, const std::function<void(bool)>& callback) {
    if (!m_running) {
        m_logger.error("DHT node not running");
        if (callback) {
            callback(false);
        }
        return;
    }

    if (!m_messageSender || !m_transactionManager) {
        m_logger.error("No message sender or transaction manager available");
        if (callback) {
            callback(false);
        }
        return;
    }

    // Create a ping query
    auto query = std::make_shared<PingQuery>("", m_nodeID);

    // Send the query
    std::string transactionID = m_transactionManager->createTransaction(
        query,
        node->getEndpoint(),
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
        m_logger.error("Failed to create transaction");
        if (callback) {
            callback(false);
        }
    }
}

} // namespace dht_hunter::dht
