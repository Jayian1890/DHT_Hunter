#include "dht_hunter/dht/transactions/components/base_transaction_component.hpp"

namespace dht_hunter::dht::transactions {

BaseTransactionComponent::BaseTransactionComponent(const std::string& name,
                                                 const DHTConfig& config,
                                                 const NodeID& nodeID)
    : m_name(name),
      m_config(config),
      m_nodeID(nodeID),
      m_eventBus(unified_event::EventBus::getInstance()),
      m_initialized(false),
      m_running(false) {
}

BaseTransactionComponent::~BaseTransactionComponent() {
    stop();
}

std::string BaseTransactionComponent::getName() const {
    return m_name;
}

bool BaseTransactionComponent::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return true;
    }

    if (!onInitialize()) {
        unified_event::logError("DHT.Transactions." + m_name, "Failed to initialize");
        return false;
    }

    m_initialized = true;
    unified_event::logTrace("DHT.Transactions." + m_name, "Initialized");
    return true;
}

bool BaseTransactionComponent::start() {
    if (!initialize()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        unified_event::logError("DHT.Transactions." + m_name, "Not initialized");
        return false;
    }

    if (m_running) {
        return true;
    }

    if (!onStart()) {
        unified_event::logError("DHT.Transactions." + m_name, "Failed to start");
        return false;
    }

    m_running = true;
    unified_event::logTrace("DHT.Transactions." + m_name, "Started");
    return true;
}

void BaseTransactionComponent::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running) {
        return;
    }

    onStop();
    m_running = false;
    unified_event::logInfo("DHT.Transactions." + m_name, "Stopped");
}

bool BaseTransactionComponent::isRunning() const {
    return m_running;
}

bool BaseTransactionComponent::onInitialize() {
    // Default implementation does nothing
    return true;
}

bool BaseTransactionComponent::onStart() {
    // Default implementation does nothing
    return true;
}

void BaseTransactionComponent::onStop() {
    // Default implementation does nothing
}

} // namespace dht_hunter::dht::transactions
