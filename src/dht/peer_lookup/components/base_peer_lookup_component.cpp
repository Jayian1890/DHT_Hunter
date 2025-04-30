#include "dht_hunter/dht/peer_lookup/components/base_peer_lookup_component.hpp"

namespace dht_hunter::dht::peer_lookup {

BasePeerLookupComponent::BasePeerLookupComponent(const std::string& name,
                                               const DHTConfig& config,
                                               const NodeID& nodeID,
                                               std::shared_ptr<RoutingTable> routingTable)
    : m_name(name),
      m_config(config),
      m_nodeID(nodeID),
      m_routingTable(routingTable),
      m_eventBus(unified_event::EventBus::getInstance()),
      m_initialized(false),
      m_running(false) {
}

BasePeerLookupComponent::~BasePeerLookupComponent() {
    stop();
}

std::string BasePeerLookupComponent::getName() const {
    return m_name;
}

bool BasePeerLookupComponent::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return true;
    }

    if (!m_routingTable) {
        unified_event::logError("DHT.PeerLookup." + m_name, "Routing table is null");
        return false;
    }

    if (!onInitialize()) {
        unified_event::logError("DHT.PeerLookup." + m_name, "Failed to initialize");
        return false;
    }

    m_initialized = true;
    unified_event::logInfo("DHT.PeerLookup." + m_name, "Initialized");
    return true;
}

bool BasePeerLookupComponent::start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        unified_event::logError("DHT.PeerLookup." + m_name, "Not initialized");
        return false;
    }

    if (m_running) {
        return true;
    }

    if (!onStart()) {
        unified_event::logError("DHT.PeerLookup." + m_name, "Failed to start");
        return false;
    }

    m_running = true;
    unified_event::logInfo("DHT.PeerLookup." + m_name, "Started");
    return true;
}

void BasePeerLookupComponent::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running) {
        return;
    }

    onStop();
    m_running = false;
    unified_event::logInfo("DHT.PeerLookup." + m_name, "Stopped");
}

bool BasePeerLookupComponent::isRunning() const {
    return m_running;
}

bool BasePeerLookupComponent::onInitialize() {
    // Default implementation does nothing
    return true;
}

bool BasePeerLookupComponent::onStart() {
    // Default implementation does nothing
    return true;
}

void BasePeerLookupComponent::onStop() {
    // Default implementation does nothing
}

} // namespace dht_hunter::dht::peer_lookup
