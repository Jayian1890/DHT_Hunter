# Unified Event System Overview

## Introduction

The DHT Hunter project now uses a unified event system that combines both logging and component communication into a single, modular system. This document provides an overview of the new event system and how to use it in your code.

## Architecture

The unified event system is built around the following key components:

1. **Event**: Base class for all events in the system
2. **EventBus**: Central hub for event distribution
3. **EventProcessor**: Interface for event processors
4. **Specific Processors**: LoggingProcessor, ComponentProcessor, StatisticsProcessor
5. **Specific Event Types**: System events, log events, node events, message events, peer events

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────────────────┐
│                 │     │                 │     │                             │
│  Event Source   │────▶│  Unified Event  │────▶│  Event Processors           │
│                 │     │      Bus        │     │  (Logging, Component,       │
└─────────────────┘     └─────────────────┘     │   Statistics)               │
                                │               │                             │
                                │               └─────────────────────────────┘
                                │                             │
                                ▼                             │
                        ┌─────────────────┐                  │
                        │                 │                  │
                        │  Subscribers    │◀─────────────────┘
                        │                 │
                        └─────────────────┘
```

## Using the Unified Event System

### Initialization

The unified event system must be initialized at application startup:

```cpp
#include "dht_hunter/unified_event/unified_event.hpp"

// Initialize the unified event system
dht_hunter::unified_event::initializeEventSystem(
    true,  // Enable logging
    true,  // Enable component communication
    true,  // Enable statistics
    false  // Synchronous processing
);

// Configure the logging processor
auto loggingProcessor = dht_hunter::unified_event::getLoggingProcessor();
if (loggingProcessor) {
    loggingProcessor->setMinSeverity(dht_hunter::unified_event::EventSeverity::Debug);
    loggingProcessor->setFileOutput(true, "dht_hunter.log");
}
```

### Logging

The unified event system provides a simple interface for logging:

```cpp
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"

// Create a logger for a component
auto logger = dht_hunter::event::Logger::forComponent("MyComponent");

// Log messages at different severity levels
logger.debug("This is a debug message");
logger.info("This is an info message with a value: {}", 42);
logger.warning("This is a warning message");
logger.error("This is an error message");
logger.critical("This is a critical message");
```

### Publishing Events

You can publish custom events to the event bus:

```cpp
#include "dht_hunter/unified_event/unified_event.hpp"
#include "dht_hunter/unified_event/events/system_events.hpp"

// Create and publish a system started event
auto event = std::make_shared<dht_hunter::unified_event::SystemStartedEvent>("MyComponent");
dht_hunter::unified_event::EventBus::getInstance()->publish(event);
```

### Subscribing to Events

You can subscribe to events to receive notifications:

```cpp
#include "dht_hunter/unified_event/unified_event.hpp"

// Get the event bus
auto eventBus = dht_hunter::unified_event::EventBus::getInstance();

// Subscribe to a specific event type
int subscriptionId = eventBus->subscribe(
    dht_hunter::unified_event::EventType::SystemStarted,
    [](std::shared_ptr<dht_hunter::unified_event::Event> event) {
        std::cout << "System started: " << event->getSource() << std::endl;
    }
);

// Subscribe to events with a specific severity
int severitySubscriptionId = eventBus->subscribeToSeverity(
    dht_hunter::unified_event::EventSeverity::Error,
    [](std::shared_ptr<dht_hunter::unified_event::Event> event) {
        std::cout << "Error event: " << event->toString() << std::endl;
    }
);

// Unsubscribe when no longer needed
eventBus->unsubscribe(subscriptionId);
eventBus->unsubscribe(severitySubscriptionId);
```

### Getting Statistics

The unified event system includes a statistics processor that collects information about events:

```cpp
#include "dht_hunter/unified_event/unified_event.hpp"

// Get the statistics processor
auto statisticsProcessor = dht_hunter::unified_event::getStatisticsProcessor();
if (statisticsProcessor) {
    // Get statistics
    size_t totalEvents = statisticsProcessor->getTotalEventCount();
    size_t errorEvents = statisticsProcessor->getSeverityCount(
        dht_hunter::unified_event::EventSeverity::Error
    );
    
    // Get all statistics as JSON
    std::string statsJson = statisticsProcessor->getStatisticsAsJson();
    std::cout << "Event Statistics: " << statsJson << std::endl;
}
```

### Shutdown

The unified event system should be shut down at application exit:

```cpp
// Shutdown the unified event system
dht_hunter::unified_event::shutdownEventSystem();
```

## Event Types

The unified event system includes the following event types:

### System Events

- **SystemStartedEvent**: Published when a system component starts
- **SystemStoppedEvent**: Published when a system component stops
- **SystemErrorEvent**: Published when a system error occurs

### Log Events

- **LogMessageEvent**: Published when a log message is generated

### Node Events

- **NodeDiscoveredEvent**: Published when a DHT node is discovered
- **NodeAddedEvent**: Published when a node is added to the routing table
- **NodeRemovedEvent**: Published when a node is removed from the routing table
- **NodeUpdatedEvent**: Published when a node is updated in the routing table

### Message Events

- **MessageReceivedEvent**: Published when a message is received
- **MessageSentEvent**: Published when a message is sent
- **MessageErrorEvent**: Published when a message error occurs

### Peer Events

- **PeerDiscoveredEvent**: Published when a peer is discovered
- **PeerAnnouncedEvent**: Published when a peer is announced

## Conclusion

The unified event system provides a powerful and flexible way to handle events in the DHT Hunter project. By combining logging and component communication into a single system, it simplifies the codebase and makes it easier to maintain and extend.
