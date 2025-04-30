#include "dht_hunter/dht/routing/routing_manager.hpp"
#include "dht_hunter/dht/transactions/transaction_manager.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include <filesystem>

namespace dht_hunter::dht {

// Initialize static members
std::shared_ptr<RoutingManager> RoutingManager::s_instance = nullptr;
std::mutex RoutingManager::s_instanceMutex;

std::shared_ptr<RoutingManager> RoutingManager::getInstance(
    const DHTConfig& config,
    const NodeID& nodeID,
    std::shared_ptr<RoutingTable> routingTable,
    std::shared_ptr<TransactionManager> transactionManager,
    std::shared_ptr<MessageSender> messageSender) {

    std::lock_guard<std::mutex> lock(s_instanceMutex);

    if (!s_instance) {
        s_instance = std::shared_ptr<RoutingManager>(new RoutingManager(
            config, nodeID, routingTable, transactionManager, messageSender));
    }

    return s_instance;
}

RoutingManager::RoutingManager(const DHTConfig& config,
                             const NodeID& nodeID,
                             std::shared_ptr<RoutingTable> routingTable,
                             std::shared_ptr<TransactionManager> transactionManager,
                             std::shared_ptr<MessageSender> messageSender)
    : m_config(config),
      m_nodeID(nodeID),
      m_routingTable(routingTable),
      m_transactionManager(transactionManager),
      m_messageSender(messageSender),
      m_running(false),
      m_eventBus(unified_event::EventBus::getInstance()) {

    // Create the node verifier component
    m_nodeVerifier = std::make_shared<routing::NodeVerifierComponent>(config, nodeID, m_routingTable, transactionManager, messageSender);

    // Create the bucket refresher component
    m_bucketRefresher = std::make_shared<routing::BucketRefreshComponent>(config, nodeID, m_routingTable,
        [this](std::shared_ptr<Node> node) {
            this->addNode(node);
        });
}

RoutingManager::~RoutingManager() {
    stop();

    // Clear the singleton instance
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (s_instance.get() == this) {
        s_instance.reset();
    }
}

bool RoutingManager::start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running) {
        return true;
    }

    // Routing table loading is now handled by PersistenceManager

    // Initialize and start the node verifier component
    if (m_nodeVerifier) {
        if (!m_nodeVerifier->initialize()) {
            unified_event::logError("DHT.RoutingManager", "Failed to initialize node verifier component");
            return false;
        }

        if (!m_nodeVerifier->start()) {
            unified_event::logError("DHT.RoutingManager", "Failed to start node verifier component");
            return false;
        }
    }

    // Initialize and start the bucket refresher component
    if (m_bucketRefresher) {
        if (!m_bucketRefresher->initialize()) {
            unified_event::logError("DHT.RoutingManager", "Failed to initialize bucket refresher component");
            return false;
        }

        if (!m_bucketRefresher->start()) {
            unified_event::logError("DHT.RoutingManager", "Failed to start bucket refresher component");
            return false;
        }
    }

    m_running = true;

    // Routing table saving is now handled by PersistenceManager

    // Publish a system started event
    auto startedEvent = std::make_shared<unified_event::SystemStartedEvent>("DHT.RoutingManager");
    m_eventBus->publish(startedEvent);
    return true;
}

void RoutingManager::stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_running) {
            return;
        }

        m_running = false;
    } // Release the lock before stopping components

    // Stop the node verifier component
    if (m_nodeVerifier) {
        m_nodeVerifier->stop();
    }

    // Stop the bucket refresher component
    if (m_bucketRefresher) {
        m_bucketRefresher->stop();
    }

    // Routing table saving is now handled by PersistenceManager

    // Publish a system stopped event
    auto stoppedEvent = std::make_shared<unified_event::SystemStoppedEvent>("DHT.RoutingManager");
    m_eventBus->publish(stoppedEvent);
}

bool RoutingManager::isRunning() const {
    return m_running;
}

std::shared_ptr<RoutingTable> RoutingManager::getRoutingTable() const {
    return m_routingTable;
}

bool RoutingManager::addNode(std::shared_ptr<Node> node) {
    if (!m_routingTable) {
        return false;
    }

    if (!m_nodeVerifier) {
        return false;
    }

    // Calculate node information for later use
    NodeID nodeID = node->getID();
    size_t bucketIndex = m_routingTable ? m_routingTable->getBucketIndex(nodeID) : 0;

    // Add the node to the verification queue with the bucket index
    return m_nodeVerifier->verifyNode(node, [this, node](bool success) {
        if (success) {

            // Get the bucket index where the node was added
            size_t addedBucketIndex = 0;
            if (m_routingTable) {
                addedBucketIndex = m_routingTable->getBucketIndex(node->getID());
            }

            // Publish a node added event
            auto addedEvent = std::make_shared<unified_event::NodeAddedEvent>("DHT.RoutingManager", node, addedBucketIndex);
            m_eventBus->publish(addedEvent);
        }
    }, bucketIndex);
}

bool RoutingManager::removeNode(const NodeID& nodeID) {
    if (!m_routingTable) {
        return false;
    }

    bool result = m_routingTable->removeNode(nodeID);

    if (result) {
    }

    return result;
}

std::shared_ptr<Node> RoutingManager::getNode(const NodeID& nodeID) const {
    if (!m_routingTable) {
        return nullptr;
    }

    return m_routingTable->getNode(nodeID);
}

std::vector<std::shared_ptr<Node>> RoutingManager::getClosestNodes(const NodeID& targetID, size_t k) const {
    if (!m_routingTable) {
        return {};
    }

    // Log the distance between our node ID and the target ID
    NodeID distance = m_nodeID.distanceTo(targetID);
    // Use the m_nodeID to avoid the unused private field warning
    std::string distanceStr = nodeIDToString(distance);

    return m_routingTable->getClosestNodes(targetID, k);
}

std::vector<std::shared_ptr<Node>> RoutingManager::getAllNodes() const {
    if (!m_routingTable) {
        return {};
    }

    return m_routingTable->getAllNodes();
}

size_t RoutingManager::getNodeCount() const {
    if (!m_routingTable) {
        return 0;
    }

    return m_routingTable->getNodeCount();
}

// Routing table saving and loading methods have been removed
// These operations are now handled by the PersistenceManager

} // namespace dht_hunter::dht
