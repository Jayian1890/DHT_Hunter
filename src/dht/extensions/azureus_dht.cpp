#include "dht_hunter/dht/extensions/azureus_dht.hpp"

namespace dht_hunter::dht::extensions {

AzureusDHT::AzureusDHT(const DHTConfig& config, 
                     const NodeID& nodeID,
                     std::shared_ptr<RoutingTable> routingTable)
    : DHTExtension(config, nodeID),
      m_routingTable(routingTable),
      m_initialized(false) {    // Logger initialization removed
}

AzureusDHT::~AzureusDHT() {
    shutdown();
}

std::string AzureusDHT::getName() const {
    return "Azureus DHT";
}

std::string AzureusDHT::getVersion() const {
    return "1.0";
}

bool AzureusDHT::initialize() {
    if (m_initialized) {
        return true;
    }

    if (!m_routingTable) {
        return false;
    }

    // TODO: Implement Azureus-specific initialization

    // Azureus DHT has some differences from Mainline DHT:
    // 1. It uses a different node ID format (still 160 bits, but generated differently)
    // 2. It uses different message types
    // 3. It has a different routing table structure
    // 
    // For this implementation, we're reusing the existing routing table
    // but in a real implementation, we would need to handle these differences

    m_initialized = true;
    return true;
}

void AzureusDHT::shutdown() {
    if (!m_initialized) {
        return;
    }
    m_initialized = false;
}

bool AzureusDHT::isInitialized() const {
    return m_initialized;
}

} // namespace dht_hunter::dht::extensions
