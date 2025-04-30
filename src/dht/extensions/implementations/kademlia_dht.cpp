#include "dht_hunter/dht/extensions/implementations/kademlia_dht.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"

namespace dht_hunter::dht::extensions {

KademliaDHT::KademliaDHT(const DHTConfig& config, 
                       const NodeID& nodeID,
                       std::shared_ptr<RoutingTable> routingTable)
    : DHTExtension(config, nodeID),
      m_routingTable(routingTable),
      m_initialized(false) {
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
        unified_event::logError("DHT.KademliaDHT", "Routing table is null");
        return false;
    }

    // TODO: Implement Kademlia-specific initialization
    unified_event::logInfo("DHT.KademliaDHT", "Initialized Kademlia DHT extension");

    m_initialized = true;
    return true;
}

void KademliaDHT::shutdown() {
    if (!m_initialized) {
        return;
    }

    // TODO: Implement Kademlia-specific shutdown
    unified_event::logInfo("DHT.KademliaDHT", "Shut down Kademlia DHT extension");

    m_initialized = false;
}

bool KademliaDHT::isInitialized() const {
    return m_initialized;
}

} // namespace dht_hunter::dht::extensions
