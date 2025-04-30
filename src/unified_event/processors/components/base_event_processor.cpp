#include "dht_hunter/unified_event/processors/components/base_event_processor.hpp"

namespace dht_hunter::unified_event {

BaseEventProcessor::BaseEventProcessor(const std::string& id)
    : m_id(id) {
}

BaseEventProcessor::~BaseEventProcessor() {
    shutdown();
}

std::string BaseEventProcessor::getId() const {
    return m_id;
}

bool BaseEventProcessor::shouldProcess(std::shared_ptr<Event> event) const {
    if (!event) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_configMutex);
    
    // Check if the event type is in the set of event types to process
    if (!m_eventTypes.empty() && m_eventTypes.find(event->getType()) == m_eventTypes.end()) {
        return false;
    }
    
    // Check if the event severity is in the set of event severities to process
    if (!m_eventSeverities.empty() && m_eventSeverities.find(event->getSeverity()) == m_eventSeverities.end()) {
        return false;
    }
    
    return true;
}

void BaseEventProcessor::process(std::shared_ptr<Event> event) {
    if (!event) {
        return;
    }
    
    onProcess(event);
}

bool BaseEventProcessor::initialize() {
    return onInitialize();
}

void BaseEventProcessor::shutdown() {
    onShutdown();
}

void BaseEventProcessor::addEventType(EventType eventType) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_eventTypes.insert(eventType);
}

void BaseEventProcessor::removeEventType(EventType eventType) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_eventTypes.erase(eventType);
}

void BaseEventProcessor::addEventSeverity(EventSeverity severity) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_eventSeverities.insert(severity);
}

void BaseEventProcessor::removeEventSeverity(EventSeverity severity) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_eventSeverities.erase(severity);
}

bool BaseEventProcessor::onInitialize() {
    return true;
}

void BaseEventProcessor::onShutdown() {
    // Default implementation does nothing
}

} // namespace dht_hunter::unified_event
