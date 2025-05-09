# DHT Core Implementation

This document provides technical details about the implementation of the DHT core components.

## Architecture

The DHT core implementation follows a modular architecture with the following key components:

1. **DHTNode**: The main entry point for DHT operations
2. **DHTConfig**: Configuration for the DHT node
3. **RoutingTable**: Manages the routing table for the DHT
4. **PersistenceManager**: Handles persistence of DHT state

## DHTNode

The DHTNode class is the main entry point for DHT operations. It provides methods for:

- Starting and stopping the DHT node
- Finding nodes in the DHT
- Sending queries to nodes
- Finding nodes with metadata for a specific info hash

### Implementation

The DHTNode class is implemented in `utils/dht_core_utils.hpp` and `utils/dht_core_utils.cpp`. The key methods are:

```cpp
// Constructor
DHTNode(const DHTConfig& config, const types::NodeID& nodeID);

// Start the DHT node
bool start();

// Stop the DHT node
void stop();

// Check if the DHT node is running
bool isRunning() const;

// Get the node ID
types::NodeID getNodeID() const;

// Get the routing table
std::shared_ptr<RoutingTable> getRoutingTable() const;

// Send a query to a node
bool sendQueryToNode(
    const types::NodeID& nodeId,
    std::shared_ptr<dht::QueryMessage> query,
    std::function<void(std::shared_ptr<dht::ResponseMessage>, bool)> callback);

// Find nodes that might have metadata for an info hash
void findNodesWithMetadata(
    const types::InfoHash& infoHash,
    std::function<void(const std::vector<std::shared_ptr<dht::Node>>&)> callback);
```

### findNodesWithMetadata Implementation

The `findNodesWithMetadata` method is implemented as follows:

```cpp
void DHTNode::findNodesWithMetadata(
    const types::InfoHash& infoHash,
    std::function<void(const std::vector<std::shared_ptr<dht::Node>>&)> callback) {

    if (!m_routingTable) {
        callback(std::vector<std::shared_ptr<dht::Node>>());
        return;
    }

    // For now, just return the closest nodes to the info hash
    // In a real implementation, we would need to track which nodes have metadata
    auto nodes = m_routingTable->getClosestNodes(types::NodeID(infoHash), 20);
    callback(nodes);
}
```

This implementation returns the 20 closest nodes to the info hash. In a more sophisticated implementation, we would track which nodes have metadata for specific info hashes.

## DHTConfig

The DHTConfig class provides configuration options for the DHT node. It is implemented in `utils/dht_core_utils.hpp` and `utils/dht_core_utils.cpp`.

```cpp
class DHTConfig {
public:
    DHTConfig();

    // Get the bootstrap nodes
    std::vector<types::Endpoint> getBootstrapNodes() const;

    // Get the K-bucket size
    size_t getKBucketSize() const;

    // Get the token rotation interval
    uint32_t getTokenRotationInterval() const;
};
```

## RoutingTable

The RoutingTable class manages the routing table for the DHT. It is implemented in `utils/dht_core_utils.hpp` and `utils/dht_core_utils.cpp`.

```cpp
class RoutingTable {
public:
    RoutingTable(const types::NodeID& nodeID, size_t kBucketSize = K_BUCKET_SIZE);

    // Add a node to the routing table
    bool addNode(std::shared_ptr<types::Node> node);

    // Find a node in the routing table
    std::shared_ptr<types::Node> findNode(const types::NodeID& nodeID) const;

    // Get the closest nodes to a node ID
    std::vector<std::shared_ptr<dht::Node>> getClosestNodes(
        const types::NodeID& nodeID, size_t count) const;

    // Get the number of nodes in the routing table
    size_t getNodeCount() const;
};
```

## PersistenceManager

The PersistenceManager class handles persistence of DHT state. It is implemented in `utils/dht_core_utils.hpp` and `utils/dht_core_utils.cpp`.

```cpp
class PersistenceManager {
public:
    static std::shared_ptr<PersistenceManager> getInstance(const std::string& configDir = ".");

    // Start the persistence manager
    bool start(
        std::shared_ptr<RoutingTable> routingTable,
        std::shared_ptr<dht::PeerStorage> peerStorage);

    // Stop the persistence manager
    void stop();

    // Save the routing table
    bool saveRoutingTable();

    // Load the routing table
    bool loadRoutingTable();

    // Save the peer storage
    bool savePeerStorage();

    // Load the peer storage
    bool loadPeerStorage();

    // Save a node ID to a file
    bool saveNodeID(const types::NodeID& nodeID);

    // Load a node ID from a file
    types::NodeID loadNodeID();
};
```

## Integration with Metadata Acquisition

The DHT core components are integrated with the metadata acquisition system through the `DHTMetadataProvider` class. This class uses the `findNodesWithMetadata` and `sendQueryToNode` methods to find nodes with metadata and send queries to them.

### Key Integration Points

1. The `DHTMetadataProvider` uses `findNodesWithMetadata` to find nodes that might have metadata for a specific info hash.
2. It then uses `sendQueryToNode` to send a `GetMetadataQuery` to each node.
3. When a response is received, it processes the metadata and stores it.

This integration allows the metadata acquisition system to leverage the DHT for finding and retrieving metadata.

### Implementation Details

The `DHTMetadataProvider` class is responsible for acquiring metadata from the DHT. It uses the following approach:

1. When a new info hash is discovered, it calls `acquireMetadata(infoHash)`.
2. The `acquireMetadata` method calls `findNodesWithMetadata` to get a list of nodes that might have metadata.
3. For each node, it creates a `GetMetadataQuery` and sends it using `sendQueryToNode`.
4. When a response is received, it processes the metadata and stores it.

This approach allows the metadata acquisition system to efficiently find and retrieve metadata from the DHT.

## Application Controller Integration

The ApplicationController was updated to work with the new DHTNode and DHTConfig classes. The key changes include:

1. Simplified the creation of the DHTNode:
   ```cpp
   // Create DHT configuration
   dht::DHTConfig dhtConfig;

   // Load or generate node ID
   types::NodeID nodeID = m_persistenceManager->loadNodeID();

   // Create and start DHT node
   m_dhtNode = std::make_shared<utils::dht_core::DHTNode>(dhtConfig, nodeID);
   ```

2. Updated the handling of routing table and peer storage:
   ```cpp
   // Start the persistence manager
   auto routingTable = m_dhtNode->getRoutingTable();
   // We don't have access to peer storage directly anymore
   if (!m_persistenceManager->start(routingTable, nullptr)) {
       unified_event::logError("Core.ApplicationController", "Failed to start persistence manager");
   }
   ```

3. Removed direct access to routing manager and peer storage:
   ```cpp
   // Create and start web server
   m_webServer = std::make_shared<web::WebServer>(
       m_webRoot,
       m_webPort,
       statsService,
       nullptr,  // No routing manager
       nullptr,  // No peer storage
       m_metadataManager
   );
   ```

These changes simplify the ApplicationController and make it work with the new consolidated DHT core components.
