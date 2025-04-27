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

std::string eventTypeToString(EventType type) {
    switch (type) {
        case EventType::SystemStarted:
            return "SystemStarted";
        case EventType::SystemStopped:
            return "SystemStopped";
        case EventType::SystemError:
            return "SystemError";
        case EventType::NodeDiscovered:
            return "NodeDiscovered";
        case EventType::NodeAdded:
            return "NodeAdded";
        case EventType::NodeRemoved:
            return "NodeRemoved";
        case EventType::NodeUpdated:
            return "NodeUpdated";
        case EventType::BucketRefreshed:
            return "BucketRefreshed";
        case EventType::BucketSplit:
            return "BucketSplit";
        case EventType::RoutingTableSaved:
            return "RoutingTableSaved";
        case EventType::RoutingTableLoaded:
            return "RoutingTableLoaded";
        case EventType::MessageSent:
            return "MessageSent";
        case EventType::MessageReceived:
            return "MessageReceived";
        case EventType::MessageError:
            return "MessageError";
        case EventType::LookupStarted:
            return "LookupStarted";
        case EventType::LookupProgress:
            return "LookupProgress";
        case EventType::LookupCompleted:
            return "LookupCompleted";
        case EventType::LookupFailed:
            return "LookupFailed";
        case EventType::PeerDiscovered:
            return "PeerDiscovered";
        case EventType::PeerAnnounced:
            return "PeerAnnounced";
        case EventType::LogMessage:
            return "LogMessage";
        case EventType::Custom:
            return "Custom";
        default:
            return "Unknown";
    }
}

std::string eventSeverityToString(EventSeverity severity) {
    switch (severity) {
        case EventSeverity::Debug:
            return "DEBUG";
        case EventSeverity::Info:
            return "INFO";
        case EventSeverity::Warning:
            return "WARNING";
        case EventSeverity::Error:
            return "ERROR";
        case EventSeverity::Critical:
            return "CRITICAL";
        default:
            return "UNKNOWN";
    }
}

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
