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
            return "Started";
        case EventType::SystemStopped:
            return "Stopped";
        case EventType::SystemError:
            return "Error";
        case EventType::NodeDiscovered:
            return "Node Discovered";
        case EventType::NodeAdded:
            return "Node Added";
        case EventType::NodeRemoved:
            return "Node Removed";
        case EventType::NodeUpdated:
            return "Node Updated";
        case EventType::BucketRefreshed:
            return "Bucket Refreshed";
        case EventType::BucketSplit:
            return "Bucket Split";
        case EventType::RoutingTableSaved:
            return "Routing Table Saved";
        case EventType::RoutingTableLoaded:
            return "Routing Table Loaded";
        case EventType::MessageSent:
            return "Message Sent";
        case EventType::MessageReceived:
            return "Message Received";
        case EventType::MessageError:
            return "Message Error";
        case EventType::LookupStarted:
            return "Lookup Started";
        case EventType::LookupProgress:
            return "Lookup Progress";
        case EventType::LookupCompleted:
            return "Lookup Completed";
        case EventType::LookupFailed:
            return "Lookup Failed";
        case EventType::PeerDiscovered:
            return "Peer Discovered";
        case EventType::PeerAnnounced:
            return "Peer Announced";
        case EventType::LogMessage:
            return "Log Message";
        case EventType::Custom:
            return "Custom Event";
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
