#include "dht_hunter/dht/node_lookup/components/base_node_lookup_component.hpp"

namespace dht_hunter::dht::node_lookup {

BaseNodeLookupComponent::BaseNodeLookupComponent(const std::string& name,
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

BaseNodeLookupComponent::~BaseNodeLookupComponent() {
    stop();
}

std::string BaseNodeLookupComponent::getName() const {
    return m_name;
}

bool BaseNodeLookupComponent::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return true;
    }

    if (!m_routingTable) {
        unified_event::logError("DHT.NodeLookup." + m_name, "Routing table is null");
        return false;
    }

    if (!onInitialize()) {
        unified_event::logError("DHT.NodeLookup." + m_name, "Failed to initialize");
        return false;
    }

    m_initialized = true;
    unified_event::logInfo("DHT.NodeLookup." + m_name, "Initialized");
    return true;
}

bool BaseNodeLookupComponent::start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        unified_event::logError("DHT.NodeLookup." + m_name, "Not initialized");
        return false;
    }

    if (m_running) {
        return true;
    }

    if (!onStart()) {
        unified_event::logError("DHT.NodeLookup." + m_name, "Failed to start");
        return false;
    }

    m_running = true;
    unified_event::logInfo("DHT.NodeLookup." + m_name, "Started");
    return true;
}

void BaseNodeLookupComponent::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running) {
        return;
    }

    onStop();
    m_running = false;
    unified_event::logInfo("DHT.NodeLookup." + m_name, "Stopped");
}

bool BaseNodeLookupComponent::isRunning() const {
    return m_running;
}

bool BaseNodeLookupComponent::onInitialize() {
    // Default implementation does nothing
    return true;
}

bool BaseNodeLookupComponent::onStart() {
    // Default implementation does nothing
    return true;
}

void BaseNodeLookupComponent::onStop() {
    // Default implementation does nothing
}

} // namespace dht_hunter::dht::node_lookup
