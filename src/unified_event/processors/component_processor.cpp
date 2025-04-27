#include "dht_hunter/unified_event/processors/component_processor.hpp"
#include <iostream>

namespace dht_hunter::unified_event {

ComponentProcessor::ComponentProcessor(const ComponentProcessorConfig& config)
    : m_config(config) {
}

ComponentProcessor::~ComponentProcessor() {
    shutdown();
}

std::string ComponentProcessor::getId() const {
    return "component";
}

bool ComponentProcessor::shouldProcess(std::shared_ptr<Event> event) const {
    if (!event) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config.eventTypes.find(event->getType()) != m_config.eventTypes.end();
}

void ComponentProcessor::process(std::shared_ptr<Event> event) {
    if (!event) {
        return;
    }
    
    // The ComponentProcessor doesn't do any additional processing
    // It simply allows the events to flow through to subscribers
    // The actual handling is done by the subscribers in the components
}

bool ComponentProcessor::initialize() {
    return true;
}

void ComponentProcessor::shutdown() {
    // Nothing to do
}

void ComponentProcessor::addEventType(EventType eventType) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config.eventTypes.insert(eventType);
}

void ComponentProcessor::removeEventType(EventType eventType) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config.eventTypes.erase(eventType);
}

bool ComponentProcessor::isProcessingEventType(EventType eventType) const {
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config.eventTypes.find(eventType) != m_config.eventTypes.end();
}

} // namespace dht_hunter::unified_event
