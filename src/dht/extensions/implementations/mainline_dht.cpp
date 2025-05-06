#include "dht_hunter/dht/extensions/implementations/mainline_dht.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"

namespace dht_hunter::dht::extensions {

MainlineDHT::MainlineDHT(const DHTConfig& config,
                       const NodeID& nodeID,
                       std::shared_ptr<RoutingTable> routingTable)
    : DHTExtension(config, nodeID),
      m_routingTable(routingTable),
      m_initialized(false) {
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
        return true;
    }

    if (!m_routingTable) {
        unified_event::logError("DHT.MainlineDHT", "Routing table is null");
        return false;
    }

    // TODO: Implement Mainline-specific initialization
    unified_event::logDebug("DHT.MainlineDHT", "Initialized Mainline DHT extension");

    m_initialized = true;
    return true;
}

void MainlineDHT::shutdown() {
    if (!m_initialized) {
        return;
    }

    // TODO: Implement Mainline-specific shutdown
    unified_event::logInfo("DHT.MainlineDHT", "Shut down Mainline DHT extension");

    m_initialized = false;
}

bool MainlineDHT::isInitialized() const {
    return m_initialized;
}

} // namespace dht_hunter::dht::extensions
