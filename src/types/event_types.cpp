#include "dht_hunter/types/event_types.hpp"

namespace dht_hunter::types {

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
            return "";
        case EventType::Custom:
            return "Custom Event";
        default:
            return "Unknown";
    }
}

std::string eventSeverityToString(EventSeverity severity) {
    switch (severity) {
        case EventSeverity::Trace:
            return "TRACE";
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

} // namespace dht_hunter::types
