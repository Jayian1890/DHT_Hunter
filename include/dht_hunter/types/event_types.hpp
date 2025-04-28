#pragma once

#include <string>

namespace dht_hunter::types {

/**
 * @brief Event severity levels
 */
enum class EventSeverity {
    Trace,      // Very detailed tracing information
    Debug,      // Detailed information for debugging
    Info,       // General information about system operation
    Warning,    // Potential issues that don't affect normal operation
    Error,      // Errors that affect operation but don't require immediate action
    Critical    // Critical errors that require immediate attention
};

/**
 * @brief Event types for the unified event system
 */
enum class EventType {
    // System events
    SystemStarted,
    SystemStopped,
    SystemError,

    // Node events
    NodeDiscovered,
    NodeAdded,
    NodeRemoved,
    NodeUpdated,

    // Routing table events
    BucketRefreshed,
    BucketSplit,
    RoutingTableSaved,
    RoutingTableLoaded,

    // Message events
    MessageSent,
    MessageReceived,
    MessageError,

    // Lookup events
    LookupStarted,
    LookupProgress,
    LookupCompleted,
    LookupFailed,

    // Peer events
    PeerDiscovered,
    PeerAnnounced,

    // Log events
    LogMessage,

    // Custom events (for extensions)
    Custom
};

/**
 * @brief Converts an event type to a string
 * @param type The event type
 * @return The string representation of the event type
 */
std::string eventTypeToString(EventType type);

/**
 * @brief Converts an event severity to a string
 * @param severity The event severity
 * @return The string representation of the event severity
 */
std::string eventSeverityToString(EventSeverity severity);

} // namespace dht_hunter::types
