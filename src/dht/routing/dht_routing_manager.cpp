#include "dht_hunter/dht/routing/dht_routing_manager.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include <filesystem>

DEFINE_COMPONENT_LOGGER("DHT", "RoutingManager")

namespace dht_hunter::dht {

DHTRoutingManager::DHTRoutingManager(const DHTNodeConfig& config, const NodeID& nodeID)
    : m_config(config),
      m_nodeID(nodeID),
      m_routingTable(nodeID, config.getKBucketSize()),
      m_routingTablePath(config.getRoutingTablePath()),
      m_running(false) {
    getLogger()->info("Routing manager created with node ID: {}", nodeIDToString(nodeID));
}

DHTRoutingManager::~DHTRoutingManager() {
    stop();
}

bool DHTRoutingManager::start() {
    if (m_running) {
        getLogger()->warning("Routing manager already running");
        return false;
    }

    // Load the routing table if it exists
    if (std::filesystem::exists(m_routingTablePath)) {
        getLogger()->info("Loading routing table from {}", m_routingTablePath);
        if (!loadRoutingTable(m_routingTablePath)) {
            getLogger()->warning("Failed to load routing table from {}", m_routingTablePath);
        }
    } else {
        getLogger()->info("No routing table file found at {}", m_routingTablePath);
    }

    // Start the save thread
    m_running = true;
    m_saveThread = std::thread(&DHTRoutingManager::saveRoutingTablePeriodically, this);

    getLogger()->info("Routing manager started");
    return true;
}

void DHTRoutingManager::stop() {
    // Use atomic operation to prevent multiple stops
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false)) {
        // Already stopped or stopping
        return;
    }

    getLogger()->info("Stopping routing manager");

    // Save the routing table before stopping
    saveRoutingTable(m_routingTablePath);

    // Join the save thread with timeout
    if (m_saveThread.joinable()) {
        getLogger()->debug("Waiting for save thread to join...");
        
        // Try to join with timeout
        auto joinStartTime = std::chrono::steady_clock::now();
        auto threadJoinTimeout = std::chrono::seconds(DEFAULT_THREAD_JOIN_TIMEOUT_SECONDS);
        
        std::thread tempThread([this]() {
            if (m_saveThread.joinable()) {
                m_saveThread.join();
            }
        });
        
        // Wait for the join thread with timeout
        if (tempThread.joinable()) {
            bool joined = false;
            while (!joined && 
                   std::chrono::steady_clock::now() - joinStartTime < threadJoinTimeout) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                joined = !m_saveThread.joinable();
            }
            
            if (joined) {
                tempThread.join();
                getLogger()->debug("Save thread joined successfully");
            } else {
                // If we couldn't join, detach both threads to avoid blocking
                getLogger()->warning("Save thread join timed out after {} seconds, detaching",
                              threadJoinTimeout.count());
                tempThread.detach();
                // We can't safely join the original thread now, so we detach it
                if (m_saveThread.joinable()) {
                    m_saveThread.detach();
                }
            }
        }
    }

    getLogger()->info("Routing manager stopped");
}

bool DHTRoutingManager::isRunning() const {
    return m_running;
}

bool DHTRoutingManager::addNode(const NodeID& nodeID, const network::EndPoint& endpoint) {
    // Don't add ourselves to the routing table
    if (nodeID == m_nodeID) {
        return false;
    }

    // Add the node to the routing table
    bool added = false;
    {
        std::lock_guard<util::CheckedMutex> lock(m_routingTableMutex);
        added = m_routingTable.addNode(nodeID, endpoint);
    }

    // Save the routing table if configured to do so
    if (added && m_config.getSaveRoutingTableOnNewNode()) {
        saveRoutingTable(m_routingTablePath);
    }

    return added;
}

bool DHTRoutingManager::removeNode(const NodeID& nodeID) {
    std::lock_guard<util::CheckedMutex> lock(m_routingTableMutex);
    return m_routingTable.removeNode(nodeID);
}

std::shared_ptr<Node> DHTRoutingManager::findNode(const NodeID& nodeID) const {
    std::lock_guard<util::CheckedMutex> lock(m_routingTableMutex);
    return m_routingTable.findNode(nodeID);
}

std::vector<std::shared_ptr<Node>> DHTRoutingManager::getClosestNodes(const NodeID& targetID, size_t count) const {
    std::lock_guard<util::CheckedMutex> lock(m_routingTableMutex);
    return m_routingTable.getClosestNodes(targetID, count);
}

size_t DHTRoutingManager::getNodeCount() const {
    std::lock_guard<util::CheckedMutex> lock(m_routingTableMutex);
    return m_routingTable.getNodeCount();
}

bool DHTRoutingManager::saveRoutingTable(const std::string& filePath) const {
    // Create the directory if it doesn't exist
    std::filesystem::path path(filePath);
    std::filesystem::create_directories(path.parent_path());

    // Save the routing table
    bool success = false;
    {
        std::lock_guard<util::CheckedMutex> lock(m_routingTableMutex);
        success = m_routingTable.saveToFile(filePath);
    }

    if (success) {
        getLogger()->info("Routing table saved to {}", filePath);
    } else {
        getLogger()->error("Failed to save routing table to {}", filePath);
    }

    return success;
}

bool DHTRoutingManager::loadRoutingTable(const std::string& filePath) {
    // Check if the file exists
    if (!std::filesystem::exists(filePath)) {
        getLogger()->error("Routing table file does not exist: {}", filePath);
        return false;
    }

    // Load the routing table
    bool success = false;
    {
        std::lock_guard<util::CheckedMutex> lock(m_routingTableMutex);
        success = m_routingTable.loadFromFile(filePath);
    }

    if (success) {
        getLogger()->info("Routing table loaded from {}", filePath);
    } else {
        getLogger()->error("Failed to load routing table from {}", filePath);
    }

    return success;
}

void DHTRoutingManager::saveRoutingTablePeriodically() {
    getLogger()->debug("Starting periodic routing table save thread");

    while (m_running) {
        // Sleep for the save interval
        std::this_thread::sleep_for(std::chrono::seconds(ROUTING_TABLE_SAVE_INTERVAL));

        // Check if we should still be running
        if (!m_running) {
            break;
        }

        // Save the routing table
        saveRoutingTable(m_routingTablePath);
    }

    getLogger()->debug("Periodic routing table save thread exiting");
}

} // namespace dht_hunter::dht
