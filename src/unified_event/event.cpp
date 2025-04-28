#include "dht_hunter/unified_event/event.hpp"
#include <sstream>
#include <iomanip>

namespace dht_hunter::unified_event {

Event::Event(EventType type, EventSeverity severity, const std::string& source)
    : m_type(type),
      m_severity(severity),
      m_source(source),
      m_timestamp(std::chrono::system_clock::now()) {
}

std::string Event::getName() const {
    return eventTypeToString(m_type);
}

std::string Event::toString() const {
    std::stringstream ss;

    // Format timestamp
    auto timeT = std::chrono::system_clock::to_time_t(m_timestamp);
    auto timeInfo = std::localtime(&timeT);
    ss << std::put_time(timeInfo, "%Y-%m-%d %H:%M:%S") << " ";

    // Add severity, source, and type
    ss << "[" << eventSeverityToString(m_severity) << "] ";
    ss << "[" << m_source << "] ";
    ss << getName();

    return ss.str();
}

// These functions are now imported from the Types module

EventSeverity stringToEventSeverity(const std::string& severityStr) {
    if (severityStr == "DEBUG") {
        return EventSeverity::Debug;
    } else if (severityStr == "INFO") {
        return EventSeverity::Info;
    } else if (severityStr == "WARNING") {
        return EventSeverity::Warning;
    } else if (severityStr == "ERROR") {
        return EventSeverity::Error;
    } else if (severityStr == "CRITICAL") {
        return EventSeverity::Critical;
    } else {
        return EventSeverity::Info; // Default
    }
}

} // namespace dht_hunter::unified_event
