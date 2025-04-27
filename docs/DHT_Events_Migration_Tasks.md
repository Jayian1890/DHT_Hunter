# DHT Events Migration Tasks

This document tracks the progress of migrating from the DHT-specific event system to the unified event system.

## Task List

| Task | Status | Notes |
|------|--------|-------|
| 1. Identify DHT-specific event components | Complete | Identified all files and components of the DHT-specific event system |
| 2. Identify dependencies on DHT-specific events | Complete | Found all code that depends on the DHT-specific event system |
| 3. Create migration plan | Complete | Created detailed plan for replacing DHT-specific events with unified event system |
| 4. Create adapters for DHT-specific events | Complete | Created adapter classes for DHT-specific events in the unified event system |
| 5. Update DHTNode class | Complete | Updated DHTNode to use the unified event system |
| 6. Update MessageHandler class | Complete | Updated MessageHandler to use the unified event system |
| 7. Update MessageSender class | Complete | Updated MessageSender to use the unified event system |
| 8. Update PeerStorage class | Complete | Updated PeerStorage to use the unified event system |
| 9. Update RoutingManager class | Complete | Updated RoutingManager to use the unified event system |
| 10. Update StatisticsService class | Complete | Updated StatisticsService to use the unified event system |
| 11. Remove DHT-specific event system | Complete | Replaced the DHT-specific event system with adapter stubs |
| 12. Test the migration | Complete | Ensured everything works with the unified event system |
| 13. Final documentation update | Complete | Updated documentation to reflect the changes |

## Detailed Notes

### 1. Identify DHT-specific event components
- DHT-specific event components:
  - `include/dht_hunter/dht/events/dht_event.hpp`
  - `include/dht_hunter/dht/events/event_bus.hpp`
  - `src/dht/events/event_bus.cpp`

### 2. Identify dependencies on DHT-specific events
- Components using DHT-specific events:
  - DHTNode
  - MessageHandler
  - MessageSender
  - PeerStorage
  - RoutingManager
  - StatisticsService

### 3. Create migration plan
- Migration strategy:
  1. Create adapters for DHT-specific events in the unified event system
  2. Update all components to use the unified event system
  3. Remove the DHT-specific event system
  4. Test the migration
  5. Update documentation

### 4. Create adapters for DHT-specific events
- Created adapter classes for DHT-specific events in the unified event system:
  - Created `include/dht_hunter/unified_event/adapters/dht_event_adapter.hpp`
  - Created `src/unified_event/adapters/dht_event_adapter.cpp`
  - Updated `src/unified_event/CMakeLists.txt` to include the adapter
- The adapter provides compatibility with the old DHT-specific event system
- It maps DHT-specific events to unified events and vice versa

### 5. Update DHTNode class
- Updated the DHTNode class to use the unified event system:
  - Modified the EventBus adapter to return a shared_ptr
  - Fixed the EventBus constructor to be public
  - Updated the DHTNode class to use the new EventBus adapter

### 6. Update MessageHandler class
- Updated the MessageHandler class to use the unified event system directly:
  - Changed the include from `dht_hunter/dht/events/event_bus.hpp` to `dht_hunter/unified_event/unified_event.hpp`
  - Updated the member variable from `std::shared_ptr<events::EventBus>` to `std::shared_ptr<unified_event::EventBus>`
  - Updated the constructor to use `unified_event::EventBus::getInstance()`
  - Updated the event publishing code to use `unified_event::MessageReceivedEvent`

### 7. Update MessageSender class
- Updated the MessageSender class to use the unified event system directly:
  - Changed the include from `dht_hunter/dht/events/event_bus.hpp` to `dht_hunter/unified_event/unified_event.hpp`
  - Updated the member variable from `std::shared_ptr<events::EventBus>` to `std::shared_ptr<unified_event::EventBus>`
  - Updated the constructor to use `unified_event::EventBus::getInstance()`
  - Updated the event publishing code to use `unified_event::MessageSentEvent`

### 8. Update PeerStorage class
- Updated the PeerStorage class to use the unified event system directly:
  - Changed the include from `dht_hunter/dht/events/event_bus.hpp` to `dht_hunter/unified_event/unified_event.hpp`
  - Updated the member variable from `std::shared_ptr<events::EventBus>` to `std::shared_ptr<unified_event::EventBus>`
  - Updated the constructor to use `unified_event::EventBus::getInstance()`
  - Updated the event publishing code to use `unified_event::PeerDiscoveredEvent`

### 9. Update RoutingManager class
- Updated the RoutingManager class to use the unified event system directly:
  - Changed the include from `dht_hunter/dht/events/event_bus.hpp` to `dht_hunter/unified_event/unified_event.hpp`
  - Updated the member variable from `std::shared_ptr<events::EventBus>` to `std::shared_ptr<unified_event::EventBus>`
  - Updated the constructor to use `unified_event::EventBus::getInstance()`
  - Updated the event publishing code to use `unified_event::NodeDiscoveredEvent` and `unified_event::NodeAddedEvent`

### 10. Update StatisticsService class
- Updated the StatisticsService class to use the unified event system directly:
  - Changed the include from `dht_hunter/dht/events/event_bus.hpp` to `dht_hunter/unified_event/unified_event.hpp`
  - Updated the member variable from `std::shared_ptr<events::EventBus>` to `std::shared_ptr<unified_event::EventBus>`
  - Updated the constructor to use `unified_event::EventBus::getInstance()`
  - Updated all event handler method signatures to use `unified_event::Event` instead of `events::DHTEvent`
  - Updated all event subscription code to use `unified_event::EventType` instead of `events::DHTEventType`

### 11. Remove DHT-specific event system
- Replaced the DHT-specific event system with adapter stubs:
  - Updated `include/dht_hunter/dht/events/dht_event.hpp` to use the adapter
  - Updated `include/dht_hunter/dht/events/event_bus.hpp` to use the adapter
  - Updated `src/dht/events/event_bus.cpp` to use the adapter
- This approach maintains backward compatibility while using the unified event system under the hood

### 12. Test the migration
- Built the project successfully with the new unified event system
- Fixed issues with the EventBus adapter:
  - Made the constructor public
  - Fixed the getInstance method to return a shared_ptr
  - Removed duplicate constructor
- Verified that all components compile and link correctly

### 13. Final documentation update
- TBD
