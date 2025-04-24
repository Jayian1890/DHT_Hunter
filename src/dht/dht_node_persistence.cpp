#include "dht_hunter/dht/dht_node.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include <chrono>
#include <thread>
#include <fstream>
#include <iostream>
#include <filesystem>

DEFINE_COMPONENT_LOGGER("DHT", "NodePersistence")

namespace dht_hunter::dht {
bool DHTNode::saveRoutingTable(const std::string& filePath) const {
    getLogger()->debug("Saving routing table to file: {}", filePath);
    // Save the node ID first
    bool nodeIdSaved = saveNodeID(filePath + ".nodeid");
    getLogger()->debug("Node ID saved: {}", nodeIdSaved ? "true" : "false");
    // Then save the routing table
    bool routingTableSaved = m_routingTable.saveToFile(filePath);
    getLogger()->debug("Routing table saved: {}", routingTableSaved ? "true" : "false");
    return routingTableSaved;
}

bool DHTNode::loadRoutingTable(const std::string& filePath) {
    getLogger()->debug("Loading routing table from file: {}", filePath);
    // First try to load the node ID
    if (loadNodeID(filePath + ".nodeid")) {
        // If we loaded the node ID, we need to recreate the routing table
        // since we can't assign to it directly (it contains a mutex)
        m_routingTable.clear();
        // The routing table will still use the old node ID internally,
        // but that's OK because we're just loading nodes from the file
    }
    // Then load the routing table
    return m_routingTable.loadFromFile(filePath);
}

bool DHTNode::loadNodeID(const std::string& filePath) {
    getLogger()->debug("Loading node ID from file: {}", filePath);
    try {
        // Check if the file exists
        std::ifstream checkFile(filePath);
        if (!checkFile) {
            getLogger()->info("Node ID file does not exist: {}", filePath);
            return false;
        }
        checkFile.close();

        // Open the file for reading
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            getLogger()->error("Failed to open node ID file for reading: {}", filePath);
            return false;
        }

        // Read the node ID
        NodeID nodeID;
        if (!file.read(reinterpret_cast<char*>(nodeID.data()), static_cast<std::streamsize>(nodeID.size()))) {
            getLogger()->error("Failed to read node ID from file: {}", filePath);
            return false;
        }

        // Update our node ID
        m_nodeID = nodeID;
        getLogger()->info("Loaded node ID from file: {}", nodeIDToString(m_nodeID));
        return true;
    } catch (const std::exception& e) {
        getLogger()->error("Exception while loading node ID from file: {}: {}", filePath, e.what());
        return false;
    }
}

bool DHTNode::saveNodeID(const std::string& filePath) const {
    getLogger()->debug("Saving node ID to file: {}", filePath);
    try {
        // Print the node ID for debugging
        getLogger()->debug("Node ID to save: {}", nodeIDToString(m_nodeID));

        // Check if the directory exists and create it if needed
        std::string directory = filePath.substr(0, filePath.find_last_of('/'));
        if (!directory.empty()) {
            getLogger()->debug("Checking directory: {}", directory);
            try {
                if (!std::filesystem::exists(directory)) {
                    getLogger()->info("Creating directory: {}", directory);
                    if (!std::filesystem::create_directories(directory)) {
                        getLogger()->error("Failed to create directory: {}", directory);
                        return false;
                    }
                }
            } catch (const std::exception& e) {
                getLogger()->error("Exception while creating directory {}: {}", directory, e.what());
                return false;
            }
        }

        // Open the file for writing
        getLogger()->debug("Opening file for writing: {}", filePath);
        std::ofstream file(filePath, std::ios::binary);
        if (!file) {
            getLogger()->error("Failed to open node ID file for writing: {}", filePath);
            return false;
        }

        // Write the node ID
        getLogger()->debug("Writing node ID to file: {} bytes", m_nodeID.size());
        file.write(reinterpret_cast<const char*>(m_nodeID.data()), static_cast<std::streamsize>(m_nodeID.size()));
        if (!file) {
            getLogger()->error("Failed to write node ID to file: {}", filePath);
            return false;
        }

        // Flush the file
        file.flush();
        if (!file) {
            getLogger()->error("Failed to flush node ID file: {}", filePath);
            return false;
        }

        // Close the file
        file.close();
        if (file.fail()) {
            getLogger()->error("Failed to close node ID file: {}", filePath);
            return false;
        }

        getLogger()->info("Saved node ID to file: {}", filePath);
        return true;
    } catch (const std::exception& e) {
        getLogger()->error("Exception while saving node ID to file: {}: {}", filePath, e.what());
        return false;
    }
}
void DHTNode::routingTableSaveThread() {
    getLogger()->debug("Routing table save thread started");
    getLogger()->debug("Routing table path: {}", m_routingTablePath);
    while (m_running) {
        // Sleep for the save interval
        std::this_thread::sleep_for(std::chrono::seconds(ROUTING_TABLE_SAVE_INTERVAL));
        // Save the routing table
        if (m_running && !m_routingTablePath.empty()) {
            getLogger()->debug("Periodically saving routing table to file: {}", m_routingTablePath);
            if (!saveRoutingTable(m_routingTablePath)) {
                getLogger()->warning("Failed to save routing table to file: {}", m_routingTablePath);
            } else {
                getLogger()->debug("Successfully saved routing table to file: {}", m_routingTablePath);
            }
        } else {
            getLogger()->debug("Skipping routing table save: running={}, path={}", m_running ? "true" : "false", m_routingTablePath);
        }
    }
    // Save one last time before exiting
    if (!m_routingTablePath.empty()) {
        getLogger()->debug("Saving routing table before exiting: {}", m_routingTablePath);
        if (!saveRoutingTable(m_routingTablePath)) {
            getLogger()->warning("Failed to save routing table to file: {}", m_routingTablePath);
        } else {
            getLogger()->debug("Successfully saved routing table before exiting: {}", m_routingTablePath);
        }
    } else {
        getLogger()->debug("Skipping final routing table save: path is empty");
    }
    getLogger()->debug("Routing table save thread stopped");
}
} // namespace dht_hunter::dht
