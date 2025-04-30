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
      m_bucketRefreshThreadRunning(false),
      m_eventBus(unified_event::EventBus::getInstance()) {

    // Create the node verifier
    m_nodeVerifier = std::make_shared<NodeVerifier>(config, nodeID, m_routingTable, transactionManager, messageSender, m_eventBus);
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

    // Start the node verifier
    if (m_nodeVerifier) {
        if (!m_nodeVerifier->start()) {
            return false;
        }
    }

    m_running = true;

    // Routing table saving is now handled by PersistenceManager

    // Start the bucket refresh thread
    startBucketRefreshThread();

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
    } // Release the lock before joining the thread

    // Stop the node verifier
    if (m_nodeVerifier) {
        m_nodeVerifier->stop();
    }

    // No save thread to wait for - saving is handled by PersistenceManager

    // Stop the bucket refresh thread
    stopBucketRefreshThread();

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
        } else {
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

void RoutingManager::startBucketRefreshThread() {
    std::lock_guard<std::mutex> lock(m_bucketRefreshMutex);

    if (m_bucketRefreshThreadRunning) {
        return;
    }

    m_bucketRefreshThreadRunning = true;
    m_bucketRefreshThread = std::thread(&RoutingManager::checkAndRefreshBuckets, this);
}

void RoutingManager::stopBucketRefreshThread() {
    {
        std::lock_guard<std::mutex> lock(m_bucketRefreshMutex);

        if (!m_bucketRefreshThreadRunning) {
            return;
        }

        m_bucketRefreshThreadRunning = false;
        m_bucketRefreshCondition.notify_all();
    } // Release the lock before joining the thread

    // Wait for the thread to finish
    if (m_bucketRefreshThread.joinable()) {
        m_bucketRefreshThread.join();
    }
}

void RoutingManager::checkAndRefreshBuckets() {
    try {
        while (m_bucketRefreshThreadRunning) {
            // Sleep for 1 minute
            {
                std::unique_lock<std::mutex> lock(m_bucketRefreshMutex);
                m_bucketRefreshCondition.wait_for(lock, std::chrono::minutes(1),
                    [this] { return !m_bucketRefreshThreadRunning; });

                if (!m_bucketRefreshThreadRunning) {
                    break;
                }
            }

            // Check if we're still running
            if (!m_running || !m_bucketRefreshThreadRunning) {
                break;
            }

            // Check all buckets
            if (m_routingTable) {
                size_t bucketCount = m_routingTable->getBucketCount();

                for (size_t i = 0; i < bucketCount; ++i) {
                    // Check if the bucket needs refreshing
                    if (m_routingTable->needsRefresh(i)) {

                        // Refresh the bucket
                        m_routingTable->refreshBucket(i, [this](const std::vector<std::shared_ptr<Node>>& nodes) {

                            // Add the nodes to the routing table
                            for (const auto& node : nodes) {
                                addNode(node);
                            }
                        });
                    }
                }
            }
        }
    } catch (const std::exception& e) {
    } catch (...) {
    }
}

} // namespace dht_hunter::dht
