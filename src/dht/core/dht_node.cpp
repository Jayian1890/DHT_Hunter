#include "dht_hunter/dht/core/dht_node.hpp"
#include "dht_hunter/dht/network/socket_manager.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include "dht_hunter/dht/network/message_handler.hpp"
#include "dht_hunter/dht/routing/routing_manager.hpp"
#include "dht_hunter/dht/routing/node_lookup.hpp"
#include "dht_hunter/dht/routing/peer_lookup.hpp"
#include "dht_hunter/dht/storage/token_manager.hpp"
#include "dht_hunter/dht/storage/peer_storage.hpp"
#include "dht_hunter/dht/transactions/transaction_manager.hpp"
#include "dht_hunter/dht/bootstrap/bootstrapper.hpp"

namespace dht_hunter::dht {

DHTNode::DHTNode(const DHTConfig& config)
    : m_nodeID(generateRandomNodeID()),
      m_config(config),
      m_running(false),
      m_logger(event::Logger::forComponent("DHT.Node")) {
    m_logger.info("Creating DHT node with ID: {}", nodeIDToString(m_nodeID));

    // Create the components
    m_routingTable = std::make_shared<RoutingTable>(m_nodeID, config.getKBucketSize());
    m_socketManager = std::make_shared<SocketManager>(config);
    m_messageSender = std::make_shared<MessageSender>(config, m_socketManager);
    m_tokenManager = std::make_shared<TokenManager>(config);
    m_peerStorage = std::make_shared<PeerStorage>(config);
    m_transactionManager = std::make_shared<TransactionManager>(config);
    m_routingManager = std::make_shared<RoutingManager>(config, m_nodeID);
    m_nodeLookup = std::make_shared<NodeLookup>(config, m_nodeID, m_routingTable, m_transactionManager, m_messageSender);
    m_peerLookup = std::make_shared<PeerLookup>(config, m_nodeID, m_routingTable, m_transactionManager, m_messageSender, m_tokenManager, m_peerStorage);
    m_bootstrapper = std::make_shared<Bootstrapper>(config, m_nodeID, m_routingManager, m_nodeLookup);
    m_messageHandler = std::make_shared<MessageHandler>(config, m_nodeID, m_messageSender, m_routingTable, m_tokenManager, m_peerStorage, m_transactionManager);
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

    m_logger.info("Starting DHT node");

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

    m_running = true;

    // Bootstrap the node
    m_bootstrapper->bootstrap([this](bool success) {
        if (success) {
            m_logger.info("DHT node bootstrapped successfully");
        } else {
            m_logger.warning("DHT node bootstrap failed");
        }
    });

    m_logger.info("DHT node started");
    return true;
}

void DHTNode::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running) {
        return;
    }

    m_logger.info("Stopping DHT node");

    // Stop the components in reverse order
    m_routingManager->stop();
    m_transactionManager->stop();
    m_peerStorage->stop();
    m_tokenManager->stop();
    m_messageSender->stop();
    m_socketManager->stop();

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

std::shared_ptr<RoutingTable> DHTNode::getRoutingTable() const {
    return m_routingTable;
}

void DHTNode::findClosestNodes(const NodeID& targetID, std::function<void(const std::vector<std::shared_ptr<Node>>&)> callback) {
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

void DHTNode::getPeers(const InfoHash& infoHash, std::function<void(const std::vector<network::EndPoint>&)> callback) {
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

void DHTNode::announcePeer(const InfoHash& infoHash, uint16_t port, std::function<void(bool)> callback) {
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

void DHTNode::ping(const std::shared_ptr<Node>& node, std::function<void(bool)> callback) {
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
        [callback](std::shared_ptr<ResponseMessage> /*response*/, const network::EndPoint& /*sender*/) {
            if (callback) {
                callback(true);
            }
        },
        [callback](std::shared_ptr<ErrorMessage> /*error*/, const network::EndPoint& /*sender*/) {
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
