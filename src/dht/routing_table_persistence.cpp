#include "dht_hunter/dht/routing_table.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include "dht_hunter/util/hash.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <iomanip>

DEFINE_COMPONENT_LOGGER("DHT", "RoutingTable")

namespace {
// Helper function to convert a node ID to a string
std::string nodeIDToString(const dht_hunter::dht::NodeID& nodeID) {
    return dht_hunter::util::bytesToHex(nodeID.data(), nodeID.size());
}
}

namespace dht_hunter::dht {
bool RoutingTable::saveToFile(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
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
        std::ofstream file(filePath, std::ios::binary);
        if (!file) {
            getLogger()->error("Failed to open file for writing: {}", filePath);
            return false;
        }
        // Write the own node ID
        file.write(reinterpret_cast<const char*>(m_ownID.data()), static_cast<std::streamsize>(m_ownID.size()));
        // Write the number of nodes
        size_t totalNodes = 0;
        for (const auto& bucket : m_buckets) {
            totalNodes += bucket.size();
        }
        file.write(reinterpret_cast<const char*>(&totalNodes), static_cast<std::streamsize>(sizeof(totalNodes)));
        // Write each node
        for (const auto& bucket : m_buckets) {
            for (const auto& node : bucket.getNodes()) {
                // Write the node ID
                file.write(reinterpret_cast<const char*>(node->getID().data()), static_cast<std::streamsize>(node->getID().size()));
                // Write the endpoint
                const auto& endpoint = node->getEndpoint();
                const auto& address = endpoint.getAddress();
                // Write the address type
                uint8_t addressType = static_cast<uint8_t>(address.getFamily());
                file.write(reinterpret_cast<const char*>(&addressType), static_cast<std::streamsize>(sizeof(addressType)));
                // Write the address
                std::string addressStr = address.toString();
                uint16_t addressLength = static_cast<uint16_t>(addressStr.length());
                file.write(reinterpret_cast<const char*>(&addressLength), static_cast<std::streamsize>(sizeof(addressLength)));
                file.write(addressStr.c_str(), static_cast<std::streamsize>(addressLength));
                // Write the port
                uint16_t port = endpoint.getPort();
                file.write(reinterpret_cast<const char*>(&port), static_cast<std::streamsize>(sizeof(port)));
                // Write whether the node is good
                uint8_t isGood = node->isGood() ? 1 : 0;
                file.write(reinterpret_cast<const char*>(&isGood), static_cast<std::streamsize>(sizeof(isGood)));
                // Write the number of failed queries
                int failedQueries = node->getFailedQueries();
                file.write(reinterpret_cast<const char*>(&failedQueries), static_cast<std::streamsize>(sizeof(failedQueries)));
            }
        }
        getLogger()->info("Saved routing table to file: {} ({} nodes)", filePath, totalNodes);
        return true;
    } catch (const std::exception& e) {
        getLogger()->error("Failed to save routing table to file: {}: {}", filePath, e.what());
        return false;
    }
}
bool RoutingTable::loadFromFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        // Check if the file exists first
        std::ifstream checkFile(filePath);
        if (!checkFile) {
            // File doesn't exist, which is normal for first run
            getLogger()->info("Routing table file does not exist: {}", filePath);
            return false;
        }
        checkFile.close();

        // Open the file for reading
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            getLogger()->error("Failed to open file for reading: {}", filePath);
            return false;
        }
        // Read the own node ID
        NodeID ownID;
        file.read(reinterpret_cast<char*>(ownID.data()), static_cast<std::streamsize>(ownID.size()));
        // Check if the own node ID matches
        if (ownID != m_ownID) {
            getLogger()->warning("Own node ID in file ({}) does not match current node ID ({}), updating",
                           nodeIDToString(ownID), nodeIDToString(m_ownID));
            // Update our own node ID to match the one in the file
            // This ensures consistency between the DHT node and the routing table
            m_ownID = ownID;
            getLogger()->info("Updated routing table own node ID to match file: {}", nodeIDToString(ownID));
        }
        // Read the number of nodes
        size_t totalNodes;
        file.read(reinterpret_cast<char*>(&totalNodes), static_cast<std::streamsize>(sizeof(totalNodes)));
        // Clear the routing table
        clear();
        // Read each node
        size_t nodesLoaded = 0;
        for (size_t i = 0; i < totalNodes; ++i) {
            // Read the node ID
            NodeID nodeID;
            file.read(reinterpret_cast<char*>(nodeID.data()), static_cast<std::streamsize>(nodeID.size()));
            // Read the address type
            uint8_t addressType;
            file.read(reinterpret_cast<char*>(&addressType), static_cast<std::streamsize>(sizeof(addressType)));
            // Read the address
            uint16_t addressLength;
            file.read(reinterpret_cast<char*>(&addressLength), static_cast<std::streamsize>(sizeof(addressLength)));
            std::string addressStr(addressLength, '\0');
            file.read(&addressStr[0], static_cast<std::streamsize>(addressLength));
            // Read the port
            uint16_t port;
            file.read(reinterpret_cast<char*>(&port), static_cast<std::streamsize>(sizeof(port)));
            // Read whether the node is good
            uint8_t isGood;
            file.read(reinterpret_cast<char*>(&isGood), static_cast<std::streamsize>(sizeof(isGood)));
            // Read the number of failed queries
            int failedQueries;
            file.read(reinterpret_cast<char*>(&failedQueries), static_cast<std::streamsize>(sizeof(failedQueries)));
            // Create the endpoint
            network::NetworkAddress address(addressStr);
            network::EndPoint endpoint(address, port);
            // Create the node
            auto node = std::make_shared<Node>(nodeID, endpoint);
            node->setGood(isGood != 0);
            // Set the failed queries
            for (int j = 0; j < failedQueries; ++j) {
                node->incrementFailedQueries();
            }
            // Add the node to the routing table
            if (addNode(node)) {
                nodesLoaded++;
            }
        }
        getLogger()->info("Loaded routing table from file: {} ({}/{} nodes)", filePath, nodesLoaded, totalNodes);
        return true;
    } catch (const std::exception& e) {
        getLogger()->error("Failed to load routing table from file: {}: {}", filePath, e.what());
        return false;
    }
}
} // namespace dht_hunter::dht
