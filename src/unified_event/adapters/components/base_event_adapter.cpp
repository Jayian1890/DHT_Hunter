#include "dht_hunter/unified_event/adapters/components/base_event_adapter.hpp"

namespace dht_hunter::unified_event {

BaseEventAdapter::BaseEventAdapter(const std::string& id)
    : m_id(id), m_initialized(false) {
}

BaseEventAdapter::~BaseEventAdapter() {
    shutdown();
}

std::string BaseEventAdapter::getId() const {
    return m_id;
}

bool BaseEventAdapter::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) {
        return true;
    }
    
    if (!onInitialize()) {
        return false;
    }
    
    m_initialized = true;
    return true;
}

void BaseEventAdapter::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        return;
    }
    
    onShutdown();
    m_initialized = false;
}

bool BaseEventAdapter::onInitialize() {
    return true;
}

void BaseEventAdapter::onShutdown() {
    // Default implementation does nothing
}

} // namespace dht_hunter::unified_event
