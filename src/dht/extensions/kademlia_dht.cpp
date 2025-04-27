#include "dht_hunter/dht/extensions/kademlia_dht.hpp"

namespace dht_hunter::dht::extensions {

KademliaDHT::KademliaDHT(const DHTConfig& config, 
                       const NodeID& nodeID,
                       std::shared_ptr<RoutingTable> routingTable)
    : DHTExtension(config, nodeID),
      m_routingTable(routingTable),
      m_initialized(false) {    // Logger initialization removed
}

KademliaDHT::~KademliaDHT() {
    shutdown();
}

std::string KademliaDHT::getName() const {
    return "Kademlia DHT";
}

std::string KademliaDHT::getVersion() const {
    return "1.0";
}

bool KademliaDHT::initialize() {
    if (m_initialized) {
        return true;
    }

    if (!m_routingTable) {
        return false;
    }

    // TODO: Implement Kademlia-specific initialization

    // Pure Kademlia implementation would use different parameters
    // For example, Kademlia typically uses k=20 for bucket size
    // and has different lookup parameters
    // However, we're reusing the existing routing table for simplicity

    m_initialized = true;
    return true;
}

void KademliaDHT::shutdown() {
    if (!m_initialized) {
        return;
    }
    m_initialized = false;
}

bool KademliaDHT::isInitialized() const {
    return m_initialized;
}

} // namespace dht_hunter::dht::extensions
