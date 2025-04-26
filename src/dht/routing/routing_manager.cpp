#include "dht_hunter/dht/routing/routing_manager.hpp"
#include <filesystem>

namespace dht_hunter::dht {

RoutingManager::RoutingManager(const DHTConfig& config, const NodeID& nodeID)
    : m_config(config),
      m_nodeID(nodeID),
      m_routingTable(std::make_shared<RoutingTable>(nodeID, config.getKBucketSize())),
      m_running(false),
      m_logger(event::Logger::forComponent("DHT.RoutingManager")) {
    m_logger.info("Creating routing manager with node ID: {}", nodeIDToString(nodeID));
}

RoutingManager::~RoutingManager() {
    stop();
}

bool RoutingManager::start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running) {
        m_logger.warning("Routing manager already running");
        return true;
    }

    // Load the routing table
    if (!m_config.getRoutingTablePath().empty()) {
        loadRoutingTable(m_config.getRoutingTablePath());
    }

    m_running = true;

    // Start the save thread
    m_saveThread = std::thread(&RoutingManager::saveRoutingTablePeriodically, this);

    m_logger.info("Routing manager started");
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

    // Wait for the save thread to finish
    if (m_saveThread.joinable()) {
        m_saveThread.join();
    }

    // Save the routing table - do this outside the lock
    if (!m_config.getRoutingTablePath().empty()) {
        saveRoutingTable(m_config.getRoutingTablePath());
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

    bool result = m_routingTable->addNode(node);

    if (result) {
        m_logger.debug("Added node {} to routing table", nodeIDToString(node->getID()));
    }

    return result;
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
    NodeID distance = calculateDistance(m_nodeID, targetID);
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

    // Create the directory if it doesn't exist
    std::filesystem::path path(filePath);
    std::filesystem::create_directories(path.parent_path());

    bool result = m_routingTable->saveToFile(filePath);

    if (result) {
        m_logger.info("Saved routing table to {}", filePath);
    } else {
        m_logger.error("Failed to save routing table to {}", filePath);
    }

    return result;
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
    while (m_running) {
        // Sleep for a while
        std::this_thread::sleep_for(std::chrono::seconds(m_config.getRoutingTableSaveInterval()));

        // Save the routing table
        if (!m_config.getRoutingTablePath().empty()) {
            saveRoutingTable(m_config.getRoutingTablePath());
        }
    }
}

} // namespace dht_hunter::dht
