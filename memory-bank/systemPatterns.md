# System Patterns: BitScrape

## System Architecture

BitScrape follows a modular, component-based architecture with an event-driven communication model. The system is designed to be highly concurrent, handling thousands of network connections simultaneously while maintaining responsiveness.

### Core Components

1. **ApplicationController**
   - Central coordinator that manages the application lifecycle
   - Initializes and configures all components
   - Handles shutdown and cleanup
   - Coordinates communication between high-level components

2. **DHT Node**
   - Implements the DHT protocol (Mainline, Kademlia, Azureus variants)
   - Manages the routing table of known nodes
   - Handles node discovery and bootstrap process
   - Processes incoming and outgoing DHT messages
   - Discovers infohashes through get_peers queries

3. **Metadata Acquisition Manager**
   - Connects to BitTorrent peers to download metadata
   - Implements the BitTorrent metadata exchange protocol
   - Manages connection pools and peer health tracking
   - Prioritizes promising infohashes for metadata download

4. **Web Server**
   - Provides a web interface for monitoring and data access
   - Displays real-time statistics and crawler status
   - Allows configuration changes during runtime
   - Provides API endpoints for external integration

5. **Unified Event System**
   - Enables communication between components through events
   - Provides a publish-subscribe mechanism for loose coupling
   - Supports both synchronous and asynchronous event handling
   - Allows components to operate independently

6. **Persistence Manager**
   - Handles data storage and retrieval
   - Manages database connections and transactions
   - Provides an abstraction layer over the storage backend
   - Implements caching for frequently accessed data

### Component Relationships

```
+---------------------+      +-------------------+
| ApplicationController|<---->| Unified Event System|
+---------------------+      +-------------------+
         |                           ^
         v                           |
+---------------------+              |
| Persistence Manager |              |
+---------------------+              |
         ^                           |
         |                           |
         v                           v
+---------------------+      +-------------------+
| DHT Node            |<---->| Metadata Acquisition|
+---------------------+      +-------------------+
         ^                           ^
         |                           |
         v                           v
+---------------------+      +-------------------+
| Network Layer       |      | Web Server        |
+---------------------+      +-------------------+
```

## Key Technical Decisions

1. **Modular Architecture**
   - Each component is an independent library
   - Components communicate through well-defined interfaces
   - Components can be used in other projects without dependencies

2. **Event-Driven Design**
   - Components communicate through events rather than direct method calls
   - Prevents operations from deadlocking the entire program
   - Allows for better separation of concerns

3. **Asynchronous Network Operations**
   - Network operations are non-blocking
   - Uses std::async for asynchronous operations
   - Callback-based approach for handling responses

4. **Resource Management**
   - Connection pools for managing peer connections
   - Rate limiting to prevent overwhelming the network
   - Prioritization of promising infohashes

5. **Cross-Platform Compatibility**
   - Abstraction layers for platform-specific code
   - Conditional compilation for platform differences
   - Platform-independent core logic

## Design Patterns in Use

1. **Factory Pattern**
   - Used for creating DHT protocol implementations
   - Allows for runtime selection of protocol variants

2. **Observer Pattern**
   - Implemented through the event system
   - Components observe events they're interested in

3. **Strategy Pattern**
   - Used for different routing strategies
   - Allows for pluggable algorithms

4. **Singleton Pattern**
   - Used sparingly for global services
   - ApplicationController is a singleton

5. **Command Pattern**
   - Used for network message handling
   - Each message type has a corresponding handler

6. **Repository Pattern**
   - Used for data access abstraction
   - Provides a clean interface to the persistence layer

## Critical Implementation Paths

1. **DHT Bootstrap Process**
   - Initial connection to the DHT network
   - Critical for starting the crawling process

2. **Node Discovery and Routing**
   - Maintaining an effective routing table
   - Essential for efficient network traversal

3. **Infohash Discovery**
   - Finding active infohashes in the DHT
   - Key to the metadata acquisition process

4. **Metadata Exchange Protocol**
   - Connecting to peers and downloading metadata
   - Core functionality of the application

5. **Event Processing Loop**
   - Handling and dispatching events
   - Central to the application's responsiveness
