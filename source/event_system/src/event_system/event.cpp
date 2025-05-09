#include "event_system/event.hpp"
#include <sstream>
#include <iomanip>

namespace event_system {

// Helper function to convert EventSeverity to string
std::string severityToString(EventSeverity severity) {
    switch (severity) {
        case EventSeverity::TRACE:    return "TRACE";
        case EventSeverity::DEBUG:    return "DEBUG";
        case EventSeverity::INFO:     return "INFO";
        case EventSeverity::WARNING:  return "WARNING";
        case EventSeverity::ERROR:    return "ERROR";
        case EventSeverity::CRITICAL: return "CRITICAL";
        default:                      return "UNKNOWN";
    }
}

// Helper function to convert EventType to string
std::string eventTypeToString(EventType type) {
    switch (type) {
        case EventType::SYSTEM:         return "SYSTEM";
        case EventType::NETWORK:        return "NETWORK";
        case EventType::DHT:            return "DHT";
        case EventType::BITTORRENT:     return "BITTORRENT";
        case EventType::USER_INTERFACE: return "UI";
        case EventType::LOG:            return "LOG";
        case EventType::CUSTOM:         return "CUSTOM";
        default:                        return "UNKNOWN";
    }
}

} // namespace event_system
