# DHT Hunter Event System and Logging Integration

## Overview

This document explains how the event-driven architecture in DHT Hunter integrates with the logging system. The event system provides a structured way to track and log significant operations throughout the DHT network, enhancing observability and debugging capabilities.

## Architecture

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│                 │     │                 │     │                 │
│  DHT Component  │────▶│    Event Bus    │────▶│  Logger System  │
│                 │     │                 │     │                 │
└─────────────────┘     └─────────────────┘     └─────────────────┘
        │                       ▲                       │
        │                       │                       │
        │                       │                       │
        ▼                       │                       ▼
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│                 │     │                 │     │                 │
│  Event Creator  │────▶│  Event Handler  │────▶│    Log Files    │
│                 │     │                 │     │                 │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

## Key Components

### 1. Event Types

The system defines various event types that represent significant operations in the DHT network:

```cpp
enum class DHTEventType {
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
    
    // System events
    SystemStarted,
    SystemStopped,
    SystemError
};
```

### 2. Event Bus

The EventBus is a singleton that manages event distribution:

```cpp
class EventBus {
public:
    static std::shared_ptr<EventBus> getInstance();
    int subscribe(DHTEventType eventType, EventCallback callback);
    bool unsubscribe(int subscriptionId);
    void publish(std::shared_ptr<DHTEvent> event);
};
```

### 3. Logger Integration

Each component that publishes or handles events has its own logger instance:

```cpp
event::Logger m_logger = event::Logger::forComponent("DHT.EventBus");
```

## Event Logging Flow

### 1. Event Creation and Publishing

When a significant operation occurs, components create and publish events:

```cpp
// Example: Publishing a node discovered event
auto node = std::make_shared<Node>(nodeID, endpoint);
auto event = std::make_shared<events::NodeDiscoveredEvent>(node);
m_eventBus->publish(event);

// The component also logs the event directly
m_logger.debug("Node discovered: {}", nodeID.toString());
```

### 2. Event Handling and Logging

Event handlers receive events and log additional information:

```cpp
void StatisticsService::handleNodeDiscoveredEvent(std::shared_ptr<events::DHTEvent> /*event*/) {
    m_nodesDiscovered++;
    m_logger.debug("Nodes discovered: {}", m_nodesDiscovered.load());
}
```

### 3. Log Level Control

The logging system allows for fine-grained control over which events are logged:

- **Debug Level**: All events are logged with detailed information
- **Info Level**: Only significant events are logged
- **Warning/Error Level**: Only problematic events are logged

## Benefits for Debugging and Monitoring

### 1. Correlation of Events

The event system allows for correlation of related events across components:

```
[DEBUG][DHT.MessageHandler] Message received from 192.168.1.10:6881
[DEBUG][DHT.RoutingManager] Node 5b7c8a... added to routing table in bucket 5
[DEBUG][DHT.StatisticsService] Nodes discovered: 127
```

### 2. Statistical Logging

The StatisticsService aggregates events and logs statistical information:

```
[INFO][DHT.StatisticsService] DHT Statistics:
  - Nodes discovered: 1,245
  - Messages received: 8,732
  - Peers discovered: 356
  - Errors: 12
```

### 3. Troubleshooting

When errors occur, the event system provides context for troubleshooting:

```
[ERROR][DHT.MessageHandler] Failed to decode message from 192.168.1.15:6881
[DEBUG][DHT.ErrorHandler] Error details: Invalid bencode format
[INFO][DHT.StatisticsService] Errors: 13
```

## Best Practices

1. **Log Context**: Always include relevant context in log messages (node IDs, message types, etc.)
2. **Appropriate Log Levels**: Use the appropriate log level for each event
3. **Avoid Excessive Logging**: For high-frequency events, consider logging only periodically
4. **Structured Logging**: Use structured logging for machine-parseable logs

## Example: Complete Event Logging Flow

```
// 1. Component creates and publishes an event
auto message = decodeMessage(data, size);
auto event = std::make_shared<MessageReceivedEvent>(message, sender);
m_eventBus->publish(event);
m_logger.debug("Received {} message from {}", message->getType(), sender.toString());

// 2. StatisticsService handles the event
void StatisticsService::handleMessageReceivedEvent(std::shared_ptr<events::DHTEvent> /*event*/) {
    m_messagesReceived++;
    
    // Only log every 100 messages to avoid spamming the log
    if (m_messagesReceived % 100 == 0) {
        m_logger.debug("Messages received: {}", m_messagesReceived.load());
    }
}

// 3. Periodic statistics logging
void StatisticsService::logStatistics() {
    m_logger.info("DHT Statistics: Nodes={}, Messages={}, Peers={}, Errors={}",
                 m_nodesDiscovered.load(),
                 m_messagesReceived.load(),
                 m_peersDiscovered.load(),
                 m_errors.load());
}
```

## Conclusion

The integration between the event system and logging system in DHT Hunter provides comprehensive visibility into the operation of the DHT network. This integration enhances debugging capabilities, enables statistical monitoring, and provides a structured approach to tracking significant operations throughout the system.
