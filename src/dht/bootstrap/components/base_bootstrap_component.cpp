#include "dht_hunter/dht/bootstrap/components/base_bootstrap_component.hpp"

namespace dht_hunter::dht::bootstrap {

BaseBootstrapComponent::BaseBootstrapComponent(const std::string& name,
                                             const DHTConfig& config,
                                             const NodeID& nodeID)
    : m_name(name),
      m_config(config),
      m_nodeID(nodeID),
      m_eventBus(unified_event::EventBus::getInstance()),
      m_initialized(false),
      m_running(false) {
}

BaseBootstrapComponent::~BaseBootstrapComponent() {
    stop();
}

std::string BaseBootstrapComponent::getName() const {
    return m_name;
}

bool BaseBootstrapComponent::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return true;
    }

    if (!onInitialize()) {
        unified_event::logError("DHT.Bootstrap." + m_name, "Failed to initialize");
        return false;
    }

    m_initialized = true;
    unified_event::logTrace("DHT.Bootstrap." + m_name, "Initialized");
    return true;
}

bool BaseBootstrapComponent::start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        unified_event::logError("DHT.Bootstrap." + m_name, "Not initialized");
        return false;
    }

    if (m_running) {
        return true;
    }

    if (!onStart()) {
        unified_event::logError("DHT.Bootstrap." + m_name, "Failed to start");
        return false;
    }

    m_running = true;
    unified_event::logTrace("DHT.Bootstrap." + m_name, "Started");
    return true;
}

void BaseBootstrapComponent::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running) {
        return;
    }

    onStop();
    m_running = false;
    unified_event::logInfo("DHT.Bootstrap." + m_name, "Stopped");
}

bool BaseBootstrapComponent::isRunning() const {
    return m_running;
}

void BaseBootstrapComponent::bootstrap(std::function<void(bool)> callback) {
    if (!m_initialized) {
        unified_event::logError("DHT.Bootstrap." + m_name, "Not initialized");
        if (callback) {
            callback(false);
        }
        return;
    }

    if (!m_running) {
        unified_event::logError("DHT.Bootstrap." + m_name, "Not running");
        if (callback) {
            callback(false);
        }
        return;
    }

    onBootstrap(callback);
}

bool BaseBootstrapComponent::onInitialize() {
    // Default implementation does nothing
    return true;
}

bool BaseBootstrapComponent::onStart() {
    // Default implementation does nothing
    return true;
}

void BaseBootstrapComponent::onStop() {
    // Default implementation does nothing
}

} // namespace dht_hunter::dht::bootstrap
