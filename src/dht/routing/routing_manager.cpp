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
      m_eventBus(unified_event::EventBus::getInstance()),
      m_logger(event::Logger::forComponent("DHT.RoutingManager")) {

    // Create the node verifier
    m_nodeVerifier = std::make_shared<NodeVerifier>(config, nodeID, m_routingTable, transactionManager, messageSender);
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
        m_logger.warning("Routing manager already running");
        return true;
    }

    // Load the routing table
    if (!m_config.getRoutingTablePath().empty()) {
        // Use the full path with config directory
        std::string fullPath = m_config.getFullPath(m_config.getRoutingTablePath());
        loadRoutingTable(fullPath);
    }

    // Start the node verifier
    if (m_nodeVerifier) {
        if (!m_nodeVerifier->start()) {
            m_logger.error("Failed to start node verifier");
            return false;
        }
    }

    m_running = true;

    // Start the save thread
    m_saveThread = std::thread(&RoutingManager::saveRoutingTablePeriodically, this);

    // Start the bucket refresh thread
    startBucketRefreshThread();

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

    // Wait for the save thread to finish
    if (m_saveThread.joinable()) {
        m_saveThread.join();
    }

    // Stop the bucket refresh thread
    stopBucketRefreshThread();

    // Save the routing table - do this outside the lock
    if (!m_config.getRoutingTablePath().empty()) {
        // Use the full path with config directory
        std::string fullPath = m_config.getFullPath(m_config.getRoutingTablePath());
        saveRoutingTable(fullPath);
    }

    m_logger.info("Routing manager stopped");
}

bool RoutingManager::isRunning() const {
    return m_running;
}

std::shared_ptr<RoutingTable> RoutingManager::getRoutingTable() const {
    return m_routingTable;
}

bool RoutingManager::addNode(std::shared_ptr<Node> node) {
    if (!m_routingTable) {
        m_logger.error("No routing table available");
        return false;
    }

    if (!m_nodeVerifier) {
        m_logger.error("No node verifier available");
        return false;
    }

    // Publish a node discovered event
    auto discoveredEvent = std::make_shared<unified_event::NodeDiscoveredEvent>("DHT.RoutingManager", node);
    m_eventBus->publish(discoveredEvent);

    // Add the node to the verification queue
    return m_nodeVerifier->verifyNode(node, [this, node](bool success) {
        if (success) {
            m_logger.debug("Node {} verified and added to routing table", nodeIDToString(node->getID()));

            // Get the bucket index where the node was added
            size_t bucketIndex = 0;
            if (m_routingTable) {
                bucketIndex = m_routingTable->getBucketIndex(node->getID());
            }

            // Publish a node added event
            auto addedEvent = std::make_shared<unified_event::NodeAddedEvent>("DHT.RoutingManager", node, bucketIndex);
            m_eventBus->publish(addedEvent);
        } else {
            m_logger.debug("Failed to verify node {}", nodeIDToString(node->getID()));
        }
    });
}

bool RoutingManager::removeNode(const NodeID& nodeID) {
    if (!m_routingTable) {
        m_logger.error("No routing table available");
        return false;
    }

    bool result = m_routingTable->removeNode(nodeID);

    if (result) {
        m_logger.debug("Removed node {} from routing table", nodeIDToString(nodeID));
    }

    return result;
}

std::shared_ptr<Node> RoutingManager::getNode(const NodeID& nodeID) const {
    if (!m_routingTable) {
        m_logger.error("No routing table available");
        return nullptr;
    }

    return m_routingTable->getNode(nodeID);
}

std::vector<std::shared_ptr<Node>> RoutingManager::getClosestNodes(const NodeID& targetID, size_t k) const {
    if (!m_routingTable) {
        m_logger.error("No routing table available");
        return {};
    }

    // Log the distance between our node ID and the target ID
    NodeID distance = m_nodeID.distanceTo(targetID);
    // Use the m_nodeID to avoid the unused private field warning
    std::string distanceStr = nodeIDToString(distance);
    m_logger.info("Distance between our node ID {} and target ID {}: {}",
                 nodeIDToString(m_nodeID), nodeIDToString(targetID), distanceStr);

    return m_routingTable->getClosestNodes(targetID, k);
}

std::vector<std::shared_ptr<Node>> RoutingManager::getAllNodes() const {
    if (!m_routingTable) {
        m_logger.error("No routing table available");
        return {};
    }

    return m_routingTable->getAllNodes();
}

size_t RoutingManager::getNodeCount() const {
    if (!m_routingTable) {
        m_logger.error("No routing table available");
        return 0;
    }

    return m_routingTable->getNodeCount();
}

bool RoutingManager::saveRoutingTable(const std::string& filePath) const {
    if (!m_routingTable) {
        m_logger.error("No routing table available");
        return false;
    }

    if (filePath.empty()) {
        m_logger.error("Empty file path provided for saving routing table");
        return false;
    }

    try {
        // Check if the parent path is valid & create the directory if it doesn't exist
        if (const std::filesystem::path path(filePath); !path.parent_path().empty()) {
            std::filesystem::create_directories(path.parent_path());
        }

        bool result = m_routingTable->saveToFile(filePath);

        if (result) {
            m_logger.info("Saved routing table to {}", filePath);
        } else {
            m_logger.error("Failed to save routing table to {}", filePath);
        }

        return result;
    } catch (const std::exception& e) {
        m_logger.error("Exception while saving routing table to {}: {}", filePath, e.what());
        return false;
    } catch (...) {
        m_logger.error("Unknown exception while saving routing table to {}", filePath);
        return false;
    }
}

bool RoutingManager::loadRoutingTable(const std::string& filePath) {
    if (!m_routingTable) {
        m_logger.error("No routing table available");
        return false;
    }

    // Check if the file exists
    if (!std::filesystem::exists(filePath)) {
        m_logger.info("Routing table file {} does not exist", filePath);
        return false;
    }

    bool result = m_routingTable->loadFromFile(filePath);

    if (result) {
        m_logger.info("Loaded routing table from {}", filePath);
    } else {
        m_logger.error("Failed to load routing table from {}", filePath);
    }

    return result;
}

void RoutingManager::saveRoutingTablePeriodically() {
    try {
        while (m_running) {
            // Sleep for 60 seconds (save every minute)
            std::this_thread::sleep_for(std::chrono::seconds(60));

            // Check if we're still running after the sleep
            if (!m_running) {
                break;
            }

            // Save the routing table
            try {
                std::string routingTablePath = m_config.getRoutingTablePath();
                if (!routingTablePath.empty()) {
                    // Use the full path with config directory
                    std::string fullPath = m_config.getFullPath(routingTablePath);
                    saveRoutingTable(fullPath);
                }
            } catch (const std::exception& e) {
                m_logger.error("Exception in periodic routing table save: {}", e.what());
                // Continue running despite the error
            } catch (...) {
                m_logger.error("Unknown exception in periodic routing table save");
                // Continue running despite the error
            }
        }
    } catch (const std::exception& e) {
        m_logger.error("Exception in routing table save thread: {}", e.what());
    } catch (...) {
        m_logger.error("Unknown exception in routing table save thread");
    }

    m_logger.info("Routing table save thread stopped");
}

void RoutingManager::startBucketRefreshThread() {
    std::lock_guard<std::mutex> lock(m_bucketRefreshMutex);

    if (m_bucketRefreshThreadRunning) {
        m_logger.warning("Bucket refresh thread already running");
        return;
    }

    m_bucketRefreshThreadRunning = true;
    m_bucketRefreshThread = std::thread(&RoutingManager::checkAndRefreshBuckets, this);

    m_logger.debug("Started bucket refresh thread");
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

    m_logger.debug("Stopped bucket refresh thread");
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
                        m_logger.debug("Refreshing bucket {}", i);

                        // Refresh the bucket
                        m_routingTable->refreshBucket(i, [this, i](const std::vector<std::shared_ptr<Node>>& nodes) {
                            m_logger.debug("Bucket {} refresh completed, found {} nodes", i, nodes.size());

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
        m_logger.error("Exception in bucket refresh thread: {}", e.what());
    } catch (...) {
        m_logger.error("Unknown exception in bucket refresh thread");
    }

    m_logger.info("Bucket refresh thread stopped");
}

} // namespace dht_hunter::dht
