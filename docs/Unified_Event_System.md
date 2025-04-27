# Unified Event System

## Overview

The Unified Event System is a comprehensive event-driven architecture that combines both logging and component communication into a single, modular system. It provides a centralized way to handle events throughout the DHT Hunter application, with support for different event types, severities, and processing strategies.

## Architecture

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

## Key Components

### 1. Event

The base class for all events in the system:

```cpp
class Event {
public:
    Event(EventType type, EventSeverity severity, const std::string& source);
    virtual ~Event() = default;
    
    EventType getType() const;
    EventSeverity getSeverity() const;
    const std::string& getSource() const;
    const std::chrono::system_clock::time_point& getTimestamp() const;
    
    virtual std::string getName() const;
    virtual std::string toString() const;
    
    template<typename T>
    void setProperty(const std::string& key, const T& value);
    
    template<typename T>
    T* getProperty(const std::string& key);
};
```

### 2. EventBus

The central hub for event distribution:

```cpp
class EventBus {
public:
    static std::shared_ptr<EventBus> getInstance(const EventBusConfig& config = EventBusConfig());
    
    int subscribe(EventType eventType, EventCallback callback);
    std::vector<int> subscribe(const std::vector<EventType>& eventTypes, EventCallback callback);
    int subscribeToSeverity(EventSeverity severity, EventCallback callback);
    bool unsubscribe(int subscriptionId);
    
    void publish(std::shared_ptr<Event> event);
    
    bool addProcessor(std::shared_ptr<EventProcessor> processor);
    bool removeProcessor(const std::string& processorId);
    std::shared_ptr<EventProcessor> getProcessor(const std::string& processorId) const;
    
    bool start();
    void stop();
    bool isRunning() const;
};
```

### 3. EventProcessor

The interface for event processors:

```cpp
class EventProcessor {
public:
    virtual ~EventProcessor() = default;
    
    virtual std::string getId() const = 0;
    virtual bool shouldProcess(std::shared_ptr<Event> event) const = 0;
    virtual void process(std::shared_ptr<Event> event) = 0;
    
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
};
```

### 4. Specific Processors

- **LoggingProcessor**: Handles logging of events to console and/or file
- **ComponentProcessor**: Facilitates communication between components
- **StatisticsProcessor**: Collects statistics about events

### 5. Specific Event Types

- **System Events**: SystemStartedEvent, SystemStoppedEvent, SystemErrorEvent
- **Log Events**: LogMessageEvent
- **Node Events**: NodeDiscoveredEvent, NodeAddedEvent, NodeRemovedEvent, NodeUpdatedEvent
- **Message Events**: MessageReceivedEvent, MessageSentEvent, MessageErrorEvent
- **Peer Events**: PeerDiscoveredEvent, PeerAnnouncedEvent

## Usage

### 1. Initialization

```cpp
// Initialize the unified event system
if (!unified_event::initializeEventSystem(
        true,  // Enable logging
        true,  // Enable component communication
        true,  // Enable statistics
        false  // Synchronous processing
    )) {
    std::cerr << "Failed to initialize the unified event system" << std::endl;
    return 1;
}
```

### 2. Publishing Events

```cpp
// Create and publish an event
auto event = std::make_shared<SystemStartedEvent>("MyComponent");
EventBus::getInstance()->publish(event);

// Or use the convenience functions for logging
unified_event::logInfo("MyComponent", "This is an info message");
unified_event::logError("MyComponent", "This is an error message");
```

### 3. Subscribing to Events

```cpp
// Subscribe to a specific event type
int subscriptionId = EventBus::getInstance()->subscribe(
    EventType::SystemStarted,
    [](std::shared_ptr<Event> event) {
        std::cout << "System started: " << event->getSource() << std::endl;
    }
);

// Subscribe to events with a specific severity
int severitySubscriptionId = EventBus::getInstance()->subscribeToSeverity(
    EventSeverity::Error,
    [](std::shared_ptr<Event> event) {
        std::cout << "Error event: " << event->toString() << std::endl;
    }
);
```

### 4. Unsubscribing

```cpp
// Unsubscribe when no longer needed
EventBus::getInstance()->unsubscribe(subscriptionId);
EventBus::getInstance()->unsubscribe(severitySubscriptionId);
```

### 5. Shutdown

```cpp
// Shutdown the unified event system
unified_event::shutdownEventSystem();
```

## Event Processors

### 1. LoggingProcessor

Handles logging of events to console and/or file:

```cpp
// Configure the logging processor
LoggingProcessorConfig loggingConfig;
loggingConfig.consoleOutput = true;
loggingConfig.fileOutput = true;
loggingConfig.logFilePath = "dht_hunter.log";
loggingConfig.minSeverity = EventSeverity::Info;

// Get the logging processor
auto loggingProcessor = unified_event::getLoggingProcessor();
if (loggingProcessor) {
    loggingProcessor->setMinSeverity(EventSeverity::Debug);
    loggingProcessor->setFileOutput(true, "custom_log.log");
}
```

### 2. ComponentProcessor

Facilitates communication between components:

```cpp
// Get the component processor
auto componentProcessor = unified_event::getComponentProcessor();
if (componentProcessor) {
    // Add a custom event type to process
    componentProcessor->addEventType(EventType::Custom);
    
    // Remove an event type from processing
    componentProcessor->removeEventType(EventType::LogMessage);
}
```

### 3. StatisticsProcessor

Collects statistics about events:

```cpp
// Get the statistics processor
auto statisticsProcessor = unified_event::getStatisticsProcessor();
if (statisticsProcessor) {
    // Get statistics
    size_t totalEvents = statisticsProcessor->getTotalEventCount();
    size_t errorEvents = statisticsProcessor->getSeverityCount(EventSeverity::Error);
    
    // Get all statistics as JSON
    std::string statsJson = statisticsProcessor->getStatisticsAsJson();
    std::cout << "Event Statistics: " << statsJson << std::endl;
    
    // Reset statistics
    statisticsProcessor->resetStatistics();
}
```

## Benefits

### 1. Unified Approach

- Single system for both logging and component communication
- Consistent event handling throughout the application
- Reduced code duplication and complexity

### 2. Modularity

- Pluggable processors for different concerns
- Easy to add new event types and processors
- Components can be added or removed without affecting others

### 3. Flexibility

- Synchronous or asynchronous event processing
- Multiple subscription mechanisms (by type, by severity)
- Configurable processing behavior

### 4. Observability

- Comprehensive statistics collection
- Centralized logging
- Easy to add monitoring and debugging

## Example

See the `examples/unified_event_example.cpp` file for a complete example of using the unified event system.

## Integration with Existing Code

The unified event system is designed to work alongside the existing logging and event systems, allowing for a gradual migration. Components can be updated one at a time to use the new system, with minimal disruption to the overall application.

To integrate with existing code:

1. Include the unified event system header:
   ```cpp
   #include "dht_hunter/unified_event/unified_event.hpp"
   ```

2. Initialize the system at application startup:
   ```cpp
   unified_event::initializeEventSystem();
   ```

3. Update components to publish and subscribe to events using the new system.

4. Shutdown the system at application shutdown:
   ```cpp
   unified_event::shutdownEventSystem();
   ```
