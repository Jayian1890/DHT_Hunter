#include "dht_hunter/dht/extensions/mainline_dht.hpp"

namespace dht_hunter::dht::extensions {

MainlineDHT::MainlineDHT(const DHTConfig& config, 
                       const NodeID& nodeID,
                       std::shared_ptr<RoutingTable> routingTable)
    : DHTExtension(config, nodeID),
      m_routingTable(routingTable),
      m_initialized(false) {
    m_logger = event::Logger::forComponent("DHT.MainlineDHT");
}

MainlineDHT::~MainlineDHT() {
    shutdown();
}

std::string MainlineDHT::getName() const {
    return "Mainline DHT";
}

std::string MainlineDHT::getVersion() const {
    return "1.0";
}

bool MainlineDHT::initialize() {
    if (m_initialized) {
        m_logger.warning("Mainline DHT extension already initialized");
        return true;
    }

    if (!m_routingTable) {
        m_logger.error("No routing table available");
        return false;
    }

    // TODO: Implement Mainline-specific initialization

    m_initialized = true;
    m_logger.info("Mainline DHT extension initialized");
    return true;
}

void MainlineDHT::shutdown() {
    if (!m_initialized) {
        return;
    }

    m_logger.info("Shutting down Mainline DHT extension");
    m_initialized = false;
}

bool MainlineDHT::isInitialized() const {
    return m_initialized;
}

} // namespace dht_hunter::dht::extensions
