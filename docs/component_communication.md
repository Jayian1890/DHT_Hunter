# Component Communication in DHT-Hunter

This document explains how components in DHT-Hunter communicate with each other without direct dependencies.

## Overview

DHT-Hunter uses two main mechanisms for inter-component communication:

1. **Event System**: For broadcasting events that multiple components might be interested in
2. **Service Locator**: For accessing services provided by other components

## Event System

The event system allows components to communicate through events without direct dependencies. Components can subscribe to events and fire events.

### Example Usage

```cpp
#include "dht_hunter/event/event_system.hpp"

// Subscribe to an event
int subscriptionId = dht_hunter::event::EventSystem::getInstance().subscribe(
    "dht.node.started",
    [](const std::any& data) {
        // Handle the event
        try {
            auto nodeId = std::any_cast<std::string>(data);
            // Do something with the node ID
        } catch (const std::bad_any_cast&) {
            // Handle error
        }
    }
);

// Fire an event
dht_hunter::event::EventSystem::getInstance().fire("dht.node.started", std::string("node123"));

// Unsubscribe from an event
dht_hunter::event::EventSystem::getInstance().unsubscribe("dht.node.started", subscriptionId);
```

## Service Locator

The service locator allows components to access services provided by other components without direct dependencies.

### Example Usage

```cpp
#include "dht_hunter/service/service_locator.hpp"

// Register a service
auto service = std::make_shared<MyService>();
dht_hunter::service::ServiceLocator::getInstance().registerService<MyService>(service);

// Get a service
auto service = dht_hunter::service::ServiceLocator::getInstance().getService<MyService>();
if (service) {
    // Use the service
    service->doSomething();
}

// Unregister a service
dht_hunter::service::ServiceLocator::getInstance().unregisterService<MyService>();
```

## Common Event Names

Here are some common event names used in DHT-Hunter:

- `dht.node.started`: Fired when a DHT node is started
- `dht.node.stopped`: Fired when a DHT node is stopped
- `dht.node.bootstrap.started`: Fired when a DHT node starts bootstrapping
- `dht.node.bootstrap.completed`: Fired when a DHT node completes bootstrapping
- `dht.node.peer.found`: Fired when a DHT node finds a peer
- `metadata.fetcher.started`: Fired when a metadata fetcher is started
- `metadata.fetcher.stopped`: Fired when a metadata fetcher is stopped
- `metadata.fetcher.metadata.found`: Fired when a metadata fetcher finds metadata
- `storage.metadata.saved`: Fired when metadata is saved to storage
- `crawler.started`: Fired when a crawler is started
- `crawler.stopped`: Fired when a crawler is stopped
- `crawler.infohash.found`: Fired when a crawler finds an infohash

## Common Service Types

Here are some common service types used in DHT-Hunter:

- `dht_hunter::dht::DHTNode`: DHT node service
- `dht_hunter::metadata::MetadataFetcher`: Metadata fetcher service
- `dht_hunter::storage::MetadataStorage`: Metadata storage service
- `dht_hunter::crawler::DHTCrawler`: DHT crawler service
- `dht_hunter::logforge::LogForge`: Logging service

## Best Practices

1. **Use Events for Notifications**: Use events to notify other components about state changes or important events.
2. **Use Services for Functionality**: Use services to provide functionality to other components.
3. **Document Events and Services**: Document the events and services your component provides and consumes.
4. **Handle Missing Services**: Always check if a service is available before using it.
5. **Unsubscribe from Events**: Always unsubscribe from events when you're done with them.
6. **Use Meaningful Event Names**: Use meaningful event names that describe the event.
7. **Use Typed Data**: Use typed data in events to make it easier to handle.
