# DHT Implementation

This document provides an overview of the DHT (Distributed Hash Table) implementation in DHT-Hunter.

## Overview

The DHT implementation follows the BitTorrent DHT protocol (BEP 5) and provides a complete, modular, and event-based DHT node that can participate in the BitTorrent DHT network.

## Components

The implementation is divided into several components:

### Core Components

- **DHTNode**: The main class that coordinates all other components
- **DHTConfig**: Configuration for the DHT node
- **DHTTypes**: Common types used throughout the DHT implementation
- **RoutingTable**: Stores information about known nodes in the DHT network

### Network Components

- **SocketManager**: Manages the UDP socket for communication
- **MessageSender**: Sends DHT messages
- **MessageHandler**: Handles incoming DHT messages
- **Message**: Base class for all DHT messages
- **QueryMessage**: DHT query messages (ping, find_node, get_peers, announce_peer)
- **ResponseMessage**: DHT response messages
- **ErrorMessage**: DHT error messages

### Routing Components

- **RoutingManager**: Manages the routing table
- **NodeLookup**: Performs node lookups
- **PeerLookup**: Performs peer lookups and announcements

### Storage Components

- **TokenManager**: Manages security tokens for announce_peer requests
- **PeerStorage**: Stores information about peers for info hashes

### Transaction Components

- **TransactionManager**: Manages DHT transactions

### Bootstrap Components

- **Bootstrapper**: Bootstraps the DHT node by connecting to known bootstrap nodes

## Usage

To use the DHT implementation, create a `DHTConfig` object and a `DHTNode` object, then start the node:

```cpp
// Create a DHT configuration
dht::DHTConfig config(6881); // Use port 6881

// Create a DHT node
auto dhtNode = std::make_shared<dht::DHTNode>(config);

// Start the DHT node
if (!dhtNode->start()) {
    // Handle error
}

// Perform a find_node lookup
dhtNode->findClosestNodes(targetID, [](const std::vector<std::shared_ptr<dht::Node>>& nodes) {
    // Handle result
});

// Perform a get_peers lookup
dhtNode->getPeers(infoHash, [](const std::vector<network::EndPoint>& peers) {
    // Handle result
});

// Announce a peer
dhtNode->announcePeer(infoHash, port, [](bool success) {
    // Handle result
});

// Stop the DHT node
dhtNode->stop();
```

## Design Principles

The implementation follows these design principles:

1. **Modularity**: Each component has a single responsibility
2. **Simplicity**: The implementation is kept as simple as possible
3. **Accuracy**: The implementation follows the DHT protocol specification exactly
4. **Event-Based**: The implementation uses events for communication between components
5. **Logging**: The implementation provides comprehensive logging for debugging and monitoring

## Testing

The implementation includes a test program (`dht_test`) that demonstrates the basic functionality of the DHT node.

To run the test program:

```
./dht_test [port]
```

The test program will:

1. Start a DHT node
2. Bootstrap the node
3. Perform a find_node lookup for a random node ID
4. Perform a get_peers lookup for a random info hash
5. Announce a peer for the random info hash
6. Run until interrupted (Ctrl+C)

## References

- [BEP 5: DHT Protocol](http://www.bittorrent.org/beps/bep_0005.html)
- [BEP 32: IPv6 extension for DHT](http://www.bittorrent.org/beps/bep_0032.html)
- [BEP 42: DHT Security Extension](http://www.bittorrent.org/beps/bep_0042.html)
