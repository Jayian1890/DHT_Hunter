# DHT Hunter Event System and Component Architecture

## Overview

This document explains how the event-driven architecture in DHT Hunter facilitates communication between components. The event system enables loose coupling between components, allowing for a more modular, maintainable, and extensible DHT implementation.

## Architecture

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│                 │     │                 │     │                 │
│  DHT Component  │────▶│    Event Bus    │────▶│  DHT Component  │
│    (Publisher)  │     │                 │     │   (Subscriber)  │
│                 │     │                 │     │                 │
└─────────────────┘     └─────────────────┘     └─────────────────┘
        │                       │                       │
        │                       │                       │
        ▼                       ▼                       ▼
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│                 │     │                 │     │                 │
│  Event Creator  │────▶│ Event Dispatcher│────▶│  Event Handler  │
│                 │     │                 │     │                 │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

## Component Interaction Model

### Traditional Direct Coupling

In a traditionally coupled system, components interact directly:

```cpp
// MessageHandler directly calls RoutingManager
void MessageHandler::handleMessage(Message message) {
    // Process message
    
    // Direct coupling to RoutingManager
    m_routingManager->addNode(message.getNodeID(), sender);
    
    // Direct coupling to PeerStorage
    if (message.hasPeers()) {
        m_peerStorage->addPeers(message.getInfoHash(), message.getPeers());
    }
}
```

### Event-Driven Decoupling

With the event system, components publish events without knowing who will handle them:

```cpp
// MessageHandler publishes events
void MessageHandler::handleMessage(Message message) {
    // Process message
    
    // Publish node discovered event
    auto nodeEvent = std::make_shared<NodeDiscoveredEvent>(message.getNodeID(), sender);
    m_eventBus->publish(nodeEvent);
    
    // Publish peer discovered event if applicable
    if (message.hasPeers()) {
        for (const auto& peer : message.getPeers()) {
            auto peerEvent = std::make_shared<PeerDiscoveredEvent>(message.getInfoHash(), peer);
            m_eventBus->publish(peerEvent);
        }
    }
}
```

## Key Components

### 1. EventBus

The central hub for event distribution:

```cpp
class EventBus {
public:
    static std::shared_ptr<EventBus> getInstance();
    int subscribe(DHTEventType eventType, EventCallback callback);
    std::vector<int> subscribe(const std::vector<DHTEventType>& eventTypes, EventCallback callback);
    bool unsubscribe(int subscriptionId);
    void publish(std::shared_ptr<DHTEvent> event);
};
```

### 2. Event Classes

Base class and specific event types:

```cpp
class DHTEvent {
public:
    explicit DHTEvent(DHTEventType type);
    virtual ~DHTEvent() = default;
    DHTEventType getType() const;
};

class NodeDiscoveredEvent : public DHTEvent {
public:
    explicit NodeDiscoveredEvent(std::shared_ptr<Node> node);
    std::shared_ptr<Node> getNode() const;
};

class MessageReceivedEvent : public DHTEvent {
public:
    MessageReceivedEvent(std::shared_ptr<Message> message, const network::NetworkAddress& sender);
    std::shared_ptr<Message> getMessage() const;
    const network::NetworkAddress& getSender() const;
};
```

### 3. Component Event Handlers

Components subscribe to events they're interested in:

```cpp
void DHTNode::subscribeToEvents() {
    // Subscribe to node discovered events
    m_eventSubscriptionIds.push_back(
        m_eventBus->subscribe(events::DHTEventType::NodeDiscovered,
            [this](std::shared_ptr<events::DHTEvent> event) {
                handleNodeDiscoveredEvent(event);
            }
        )
    );
    
    // Subscribe to peer discovered events
    m_eventSubscriptionIds.push_back(
        m_eventBus->subscribe(events::DHTEventType::PeerDiscovered,
            [this](std::shared_ptr<events::DHTEvent> event) {
                handlePeerDiscoveredEvent(event);
            }
        )
    );
}
```

## Component Integration

### 1. Core Components

Core DHT components that integrate with the event system:

| Component | Publisher Role | Subscriber Role |
|-----------|----------------|-----------------|
| DHTNode | System events | Node, Peer, Error events |
| MessageHandler | Message, Node, Peer events | - |
| RoutingManager | Node events | - |
| PeerStorage | Peer events | - |
| StatisticsService | - | All events |

### 2. Event Flow Between Components

Example flow for node discovery:

1. **MessageHandler** receives a message and publishes a NodeDiscoveredEvent
2. **RoutingManager** subscribes to NodeDiscoveredEvent and adds the node to the routing table
3. **StatisticsService** subscribes to NodeDiscoveredEvent and updates node discovery statistics
4. **DHTNode** subscribes to NodeDiscoveredEvent for high-level monitoring

## Benefits for Component Architecture

### 1. Loose Coupling

Components don't need direct references to each other:

```cpp
// Before: Direct coupling
class MessageHandler {
private:
    std::shared_ptr<RoutingManager> m_routingManager;
    std::shared_ptr<PeerStorage> m_peerStorage;
};

// After: Event-based coupling
class MessageHandler {
private:
    std::shared_ptr<events::EventBus> m_eventBus;
};
```

### 2. Extensibility

New components can be added without modifying existing ones:

```cpp
// New component that uses existing events
class NetworkAnalyzer {
public:
    NetworkAnalyzer() {
        m_eventBus = events::EventBus::getInstance();
        
        // Subscribe to existing events without changing their publishers
        m_eventBus->subscribe(events::DHTEventType::MessageReceived,
            [this](std::shared_ptr<events::DHTEvent> event) {
                analyzeMessage(event);
            }
        );
    }
};
```

### 3. Testability

Components can be tested in isolation using mock events:

```cpp
// Testing a component with mock events
TEST_CASE("RoutingManager handles node discovery") {
    auto routingManager = std::make_shared<RoutingManager>();
    auto mockNode = std::make_shared<Node>(generateRandomNodeID(), mockEndpoint);
    
    // Create and publish a mock event
    auto event = std::make_shared<NodeDiscoveredEvent>(mockNode);
    EventBus::getInstance()->publish(event);
    
    // Verify the routing manager added the node
    REQUIRE(routingManager->containsNode(mockNode->getID()));
}
```

## Implementation Patterns

### 1. Singleton Event Bus

The EventBus is implemented as a singleton for global access:

```cpp
std::shared_ptr<EventBus> EventBus::getInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    
    if (!s_instance) {
        s_instance = std::shared_ptr<EventBus>(new EventBus());
    }
    
    return s_instance;
}
```

### 2. Component Lifecycle Management

Components manage their event subscriptions during their lifecycle:

```cpp
// During startup
void DHTNode::start() {
    // Subscribe to events
    subscribeToEvents();
}

// During shutdown
void DHTNode::stop() {
    // Unsubscribe from events
    for (int subscriptionId : m_eventSubscriptionIds) {
        m_eventBus->unsubscribe(subscriptionId);
    }
    m_eventSubscriptionIds.clear();
}
```

### 3. Microservice Integration

The event system enables microservice-style components:

```cpp
// StatisticsService as a microservice
class StatisticsService {
public:
    static std::shared_ptr<StatisticsService> getInstance();
    
    bool start();
    void stop();
    
    // API for other components
    size_t getNodesDiscovered() const;
    std::string getStatisticsAsJson() const;
};
```

## Example: Complete Component Interaction

```
// 1. MessageHandler receives a message and publishes an event
void MessageHandler::handleRawMessage(const uint8_t* data, size_t size, const network::EndPoint& sender) {
    auto message = Message::decode(data, size);
    
    // Publish message received event
    auto event = std::make_shared<MessageReceivedEvent>(message, sender.getAddress());
    m_eventBus->publish(event);
}

// 2. RoutingManager handles node discovery events
void RoutingManager::handleNodeDiscoveredEvent(std::shared_ptr<events::DHTEvent> event) {
    auto nodeEvent = std::dynamic_pointer_cast<NodeDiscoveredEvent>(event);
    auto node = nodeEvent->getNode();
    
    // Add the node to the routing table
    addNode(node);
}

// 3. StatisticsService tracks events
void StatisticsService::handleNodeDiscoveredEvent(std::shared_ptr<events::DHTEvent> /*event*/) {
    m_nodesDiscovered++;
    m_logger.debug("Nodes discovered: {}", m_nodesDiscovered.load());
}

// 4. DHTNode provides high-level API access to statistics
std::string DHTNode::getStatistics() const {
    if (m_statisticsService) {
        return m_statisticsService->getStatisticsAsJson();
    }
    return "{}";
}
```

## Best Practices

1. **Appropriate Event Granularity**: Define events at the right level of granularity
2. **Event Payload Design**: Include all necessary information in events, but avoid excessive data
3. **Error Handling**: Handle exceptions in event handlers to prevent cascading failures
4. **Subscription Management**: Properly manage subscriptions during component lifecycle
5. **Thread Safety**: Ensure thread-safe event publishing and handling

## Conclusion

The event-driven architecture in DHT Hunter provides a powerful mechanism for component interaction. By decoupling components through events, the system achieves greater modularity, extensibility, and testability. This approach aligns with modern software design principles and enables a more maintainable DHT implementation.
