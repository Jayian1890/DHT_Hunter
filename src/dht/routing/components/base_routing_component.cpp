#include "dht_hunter/dht/routing/components/base_routing_component.hpp"

namespace dht_hunter::dht::routing {

BaseRoutingComponent::BaseRoutingComponent(const std::string& name,
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

BaseRoutingComponent::~BaseRoutingComponent() {
    stop();
}

std::string BaseRoutingComponent::getName() const {
    return m_name;
}

bool BaseRoutingComponent::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return true;
    }

    if (!m_routingTable) {
        unified_event::logError("DHT.Routing." + m_name, "Routing table is null");
        return false;
    }

    if (!onInitialize()) {
        unified_event::logError("DHT.Routing." + m_name, "Failed to initialize");
        return false;
    }

    m_initialized = true;
    unified_event::logTrace("DHT.Routing." + m_name, "Initialized");
    return true;
}

bool BaseRoutingComponent::start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        unified_event::logError("DHT.Routing." + m_name, "Not initialized");
        return false;
    }

    if (m_running) {
        return true;
    }

    if (!onStart()) {
        unified_event::logError("DHT.Routing." + m_name, "Failed to start");
        return false;
    }

    m_running = true;
    unified_event::logTrace("DHT.Routing." + m_name, "Started");
    return true;
}

void BaseRoutingComponent::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running) {
        return;
    }

    onStop();
    m_running = false;
    unified_event::logInfo("DHT.Routing." + m_name, "Stopped");
}

bool BaseRoutingComponent::isRunning() const {
    return m_running;
}

bool BaseRoutingComponent::onInitialize() {
    // Default implementation does nothing
    return true;
}

bool BaseRoutingComponent::onStart() {
    // Default implementation does nothing
    return true;
}

void BaseRoutingComponent::onStop() {
    // Default implementation does nothing
}

} // namespace dht_hunter::dht::routing
