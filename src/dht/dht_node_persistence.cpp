#include "dht_hunter/dht/dht_node.hpp"
#include "dht_hunter/logforge/logforge.hpp"

#include <chrono>
#include <thread>

namespace dht_hunter::dht {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("DHT.NodePersistence");
}

bool DHTNode::saveRoutingTable(const std::string& filePath) const {
    logger->debug("Saving routing table to file: {}", filePath);
    return m_routingTable.saveToFile(filePath);
}

bool DHTNode::loadRoutingTable(const std::string& filePath) {
    logger->debug("Loading routing table from file: {}", filePath);
    return m_routingTable.loadFromFile(filePath);
}

void DHTNode::routingTableSaveThread() {
    logger->debug("Routing table save thread started");

    while (m_running) {
        // Sleep for the save interval
        std::this_thread::sleep_for(std::chrono::seconds(ROUTING_TABLE_SAVE_INTERVAL));

        // Save the routing table
        if (m_running && !m_routingTablePath.empty()) {
            logger->debug("Periodically saving routing table to file: {}", m_routingTablePath);
            if (!saveRoutingTable(m_routingTablePath)) {
                logger->warning("Failed to save routing table to file: {}", m_routingTablePath);
            }
        }
    }

    // Save one last time before exiting
    if (!m_routingTablePath.empty()) {
        logger->debug("Saving routing table before exiting: {}", m_routingTablePath);
        if (!saveRoutingTable(m_routingTablePath)) {
            logger->warning("Failed to save routing table to file: {}", m_routingTablePath);
        }
    }

    logger->debug("Routing table save thread stopped");
}

} // namespace dht_hunter::dht
