# DHT Hunter Refactoring Plan

This document outlines the plan for reorganizing, optimizing, and modularizing the DHT code without changing its functionality. The goal is to break down the large `dht_node.cpp` file into smaller, more focused components with clear responsibilities.

## Optimized Directory Structure

```
include/dht_hunter/dht/
├── core/
│   ├── dht_node.hpp                  # Main interface (slimmed down)
│   ├── dht_node_config.hpp           # Configuration structures
│   ├── dht_types.hpp                 # Common types and constants
│   └── dht_constants.hpp             # Constants extracted from dht_node.hpp
├── network/
│   ├── dht_socket_manager.hpp        # Socket management
│   ├── dht_message_sender.hpp        # Message sending
│   └── dht_message_handler.hpp       # Message handling
├── routing/
│   ├── dht_routing_manager.hpp       # Routing table management
│   ├── dht_node_lookup.hpp           # Node lookup operations
│   └── dht_peer_lookup.hpp           # Peer lookup operations
├── storage/
│   ├── dht_peer_storage.hpp          # Peer storage
│   └── dht_token_manager.hpp         # Token management
├── transactions/
│   ├── dht_transaction_manager.hpp   # Transaction management
│   └── dht_transaction_types.hpp     # Transaction-related types
└── persistence/
    └── dht_persistence_manager.hpp   # Persistence management
```

```
src/dht/
├── core/
│   ├── dht_node.cpp
│   ├── dht_node_config.cpp
│   └── dht_constants.cpp
├── network/
│   ├── dht_socket_manager.cpp
│   ├── dht_message_sender.cpp
│   └── dht_message_handler.cpp
├── routing/
│   ├── dht_routing_manager.cpp
│   ├── dht_node_lookup.cpp
│   └── dht_peer_lookup.cpp
├── storage/
│   ├── dht_peer_storage.cpp
│   └── dht_token_manager.cpp
├── transactions/
│   ├── dht_transaction_manager.cpp
│   └── dht_transaction_persistence.cpp
└── persistence/
    └── dht_persistence_manager.cpp
```

## Refactoring Progress Tracker

### Phase 1: Setup and Interface Definition

| Task | Status | Notes |
|------|--------|-------|
| Create new directory structure | Completed | Created core, network, routing, storage, transactions, and persistence directories in both include and src paths |
| Define component interfaces | Completed | Created DHTTokenManager interface and implementation |
| Update CMakeLists.txt | Completed | Added new components to the build system |
| Create dht_constants.hpp | Completed | Extracted constants from dht_node.hpp and added additional constants for configuration |
| Create dht_node_config.hpp | Completed | Created configuration class with getters/setters and validation |

### Phase 2: Core Components

| Component | Status | Dependencies | Notes |
|-----------|--------|--------------|-------|
| dht_node.hpp/cpp | Completed | All components | Created implementation with all components integrated: socket, message, message handler, routing manager, peer storage, transaction manager, node lookup, peer lookup, and persistence manager |
| dht_node_config.hpp/cpp | Completed | None | Configuration structures and validation |
| dht_constants.hpp/cpp | Completed | None | Constants and default values |
| dht_types.hpp/cpp | Completed | None | Core DHT types |

### Phase 3: Network Components

| Component | Status | Dependencies | Notes |
|-----------|--------|--------------|-------|
| dht_socket_manager.hpp/cpp | Completed | dht_node_config | Socket creation, binding, receiving |
| dht_message_sender.hpp/cpp | Completed | dht_socket_manager | Message encoding and sending (temporarily without message batching) |
| dht_message_handler.hpp/cpp | Completed | dht_message_sender, dht_routing_manager, dht_transaction_manager | Message processing |

### Phase 4: Routing Components

| Component | Status | Dependencies | Notes |
|-----------|--------|--------------|-------|
| dht_routing_manager.hpp/cpp | Completed | dht_node_config | Routing table management |
| dht_node_lookup.hpp/cpp | Completed | dht_routing_manager, dht_transaction_manager | Node lookup implementation |
| dht_peer_lookup.hpp/cpp | Completed | dht_routing_manager, dht_transaction_manager, dht_peer_storage, dht_token_manager | Peer lookup implementation |

### Phase 5: Storage Components

| Component | Status | Dependencies | Notes |
|-----------|--------|--------------|-------|
| dht_peer_storage.hpp/cpp | Completed | dht_node_config | Peer storage and cleanup |
| dht_token_manager.hpp/cpp | Completed | None | Token generation and validation |

### Phase 6: Transaction Components

| Component | Status | Dependencies | Notes |
|-----------|--------|--------------|-------|
| dht_transaction_manager.hpp/cpp | Completed | dht_node_config | Transaction tracking and timeouts |
| dht_transaction_types.hpp | Completed | None | Transaction-related types |
| dht_transaction_persistence.hpp/cpp | In Progress | dht_transaction_manager | Transaction persistence - needs bencode library update |

### Phase 7: Persistence Components

| Component | Status | Dependencies | Notes |
|-----------|--------|--------------|-------|
| dht_persistence_manager.hpp/cpp | Completed | dht_routing_manager, dht_peer_storage, dht_transaction_manager | Coordinating persistence operations |

### Phase 8: Testing and Integration

| Task | Status | Notes |
|------|--------|-------|
| Unit tests for each component | Not Started | |
| Integration tests | Not Started | |
| Performance testing | Not Started | |
| Documentation | Not Started | |

## Component Details

### DHTNode (Core Interface)

**Responsibilities:**
- Provide public API for DHT functionality
- Initialize and coordinate components
- Manage component lifecycle

**Key Methods:**
- Constructor/Destructor
- start()/stop()
- bootstrap()
- High-level DHT operations (findNode, getPeers, etc.)

### DHTSocketManager

**Responsibilities:**
- Create and manage UDP socket
- Handle socket binding and closing
- Manage receive thread
- Process incoming messages

**Key Methods:**
- start()/stop()
- receiveMessages()
- isRunning()

### DHTMessageSender

**Responsibilities:**
- Encode and send DHT messages
- Manage message batching
- Handle send errors

**Key Methods:**
- sendQuery()
- sendResponse()
- sendError()

### DHTMessageHandler

**Responsibilities:**
- Process incoming messages
- Route messages to appropriate handlers
- Update routing table based on messages

**Key Methods:**
- handleMessage()
- handleQuery()/handleResponse()/handleError()
- Query-specific handlers (handlePingQuery, etc.)

### DHTRoutingManager

**Responsibilities:**
- Manage routing table (k-buckets)
- Add/remove/find nodes
- Get closest nodes to target
- Persistence of routing table

**Key Methods:**
- addNode()/removeNode()/findNode()
- getClosestNodes()
- saveRoutingTable()/loadRoutingTable()

### DHTNodeLookup

**Responsibilities:**
- Implement iterative node lookup
- Track lookup state
- Process lookup responses

**Key Methods:**
- startNodeLookup()
- continueNodeLookup()
- handleNodeLookupResponse()
- completeNodeLookup()

### DHTPeerLookup

**Responsibilities:**
- Implement get_peers lookup
- Track lookup state
- Process lookup responses

**Key Methods:**
- startGetPeersLookup()
- continueGetPeersLookup()
- handleGetPeersLookupResponse()
- completeGetPeersLookup()

### DHTPeerStorage

**Responsibilities:**
- Store and retrieve peers
- Clean up expired peers
- Persistence of peer cache

**Key Methods:**
- storePeer()/getStoredPeers()
- cleanupExpiredPeers()
- savePeerCache()/loadPeerCache()

### DHTTokenManager

**Responsibilities:**
- Generate and validate tokens
- Rotate secrets periodically
- Secure token handling

**Key Methods:**
- generateToken()
- validateToken()
- rotateSecret()

### DHTTransactionManager

**Responsibilities:**
- Create and track transactions
- Handle transaction timeouts
- Manage transaction queue
- Persistence of transactions

**Key Methods:**
- addTransaction()/removeTransaction()/findTransaction()
- checkTimeouts()
- processQueue()
- saveTransactions()/loadTransactions()

### DHTPersistenceManager

**Responsibilities:**
- Coordinate saving/loading of all persistent data
- Manage file paths and directories
- Handle persistence errors

**Key Methods:**
- ensureDirectoriesExist()
- saveAll()/loadAll()
- schedulePeriodicSave()

## Implementation Strategy

1. **Incremental Approach**
   - Implement one component at a time
   - Maintain backward compatibility
   - Test after each component is implemented

2. **Testing Strategy**
   - Unit tests for each component
   - Integration tests for component interactions
   - End-to-end tests for full functionality

3. **Documentation**
   - Document public API
   - Document component interactions
   - Update existing documentation

## Timeline

| Phase | Estimated Time | Dependencies |
|-------|----------------|--------------|
| Phase 1: Setup | 1 day | None |
| Phase 2: Core Components | 2 days | Phase 1 |
| Phase 3: Network Components | 3 days | Phase 2 |
| Phase 4: Routing Components | 3 days | Phase 3 |
| Phase 5: Storage Components | 2 days | Phase 2 |
| Phase 6: Transaction Components | 3 days | Phase 2 |
| Phase 7: Persistence Components | 2 days | Phases 2-6 |
| Phase 8: Testing and Integration | 5 days | Phases 1-7 |

Total estimated time: 21 days
