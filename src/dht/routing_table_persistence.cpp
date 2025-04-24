#include "dht_hunter/dht/routing_table.hpp"
#include "dht_hunter/logforge/logforge.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

namespace dht_hunter::dht {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("DHT.RoutingTable");
}

bool RoutingTable::saveToFile(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
        // Open the file for writing
        std::ofstream file(filePath, std::ios::binary);
        if (!file) {
            logger->error("Failed to open file for writing: {}", filePath);
            return false;
        }
        
        // Write the own node ID
        file.write(reinterpret_cast<const char*>(m_ownID.data()), m_ownID.size());
        
        // Write the number of nodes
        size_t totalNodes = 0;
        for (const auto& bucket : m_buckets) {
            totalNodes += bucket.size();
        }
        file.write(reinterpret_cast<const char*>(&totalNodes), sizeof(totalNodes));
        
        // Write each node
        for (const auto& bucket : m_buckets) {
            for (const auto& node : bucket.getNodes()) {
                // Write the node ID
                file.write(reinterpret_cast<const char*>(node->getID().data()), node->getID().size());
                
                // Write the endpoint
                const auto& endpoint = node->getEndpoint();
                const auto& address = endpoint.getAddress();
                
                // Write the address type
                uint8_t addressType = static_cast<uint8_t>(address.getFamily());
                file.write(reinterpret_cast<const char*>(&addressType), sizeof(addressType));
                
                // Write the address
                std::string addressStr = address.toString();
                uint16_t addressLength = static_cast<uint16_t>(addressStr.length());
                file.write(reinterpret_cast<const char*>(&addressLength), sizeof(addressLength));
                file.write(addressStr.c_str(), addressLength);
                
                // Write the port
                uint16_t port = endpoint.getPort();
                file.write(reinterpret_cast<const char*>(&port), sizeof(port));
                
                // Write whether the node is good
                uint8_t isGood = node->isGood() ? 1 : 0;
                file.write(reinterpret_cast<const char*>(&isGood), sizeof(isGood));
                
                // Write the number of failed queries
                int failedQueries = node->getFailedQueries();
                file.write(reinterpret_cast<const char*>(&failedQueries), sizeof(failedQueries));
            }
        }
        
        logger->info("Saved routing table to file: {} ({} nodes)", filePath, totalNodes);
        return true;
    } catch (const std::exception& e) {
        logger->error("Failed to save routing table to file: {}: {}", filePath, e.what());
        return false;
    }
}

bool RoutingTable::loadFromFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
        // Open the file for reading
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            logger->error("Failed to open file for reading: {}", filePath);
            return false;
        }
        
        // Read the own node ID
        NodeID ownID;
        file.read(reinterpret_cast<char*>(ownID.data()), ownID.size());
        
        // Check if the own node ID matches
        if (ownID != m_ownID) {
            logger->warning("Own node ID in file does not match current node ID");
            // We'll still load the nodes, but this is a warning
        }
        
        // Read the number of nodes
        size_t totalNodes;
        file.read(reinterpret_cast<char*>(&totalNodes), sizeof(totalNodes));
        
        // Clear the routing table
        clear();
        
        // Read each node
        size_t nodesLoaded = 0;
        for (size_t i = 0; i < totalNodes; ++i) {
            // Read the node ID
            NodeID nodeID;
            file.read(reinterpret_cast<char*>(nodeID.data()), nodeID.size());
            
            // Read the address type
            uint8_t addressType;
            file.read(reinterpret_cast<char*>(&addressType), sizeof(addressType));
            
            // Read the address
            uint16_t addressLength;
            file.read(reinterpret_cast<char*>(&addressLength), sizeof(addressLength));
            std::string addressStr(addressLength, '\0');
            file.read(&addressStr[0], addressLength);
            
            // Read the port
            uint16_t port;
            file.read(reinterpret_cast<char*>(&port), sizeof(port));
            
            // Read whether the node is good
            uint8_t isGood;
            file.read(reinterpret_cast<char*>(&isGood), sizeof(isGood));
            
            // Read the number of failed queries
            int failedQueries;
            file.read(reinterpret_cast<char*>(&failedQueries), sizeof(failedQueries));
            
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
        
        logger->info("Loaded routing table from file: {} ({}/{} nodes)", filePath, nodesLoaded, totalNodes);
        return true;
    } catch (const std::exception& e) {
        logger->error("Failed to load routing table from file: {}: {}", filePath, e.what());
        return false;
    }
}

} // namespace dht_hunter::dht
