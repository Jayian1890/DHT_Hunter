# DHT-Hunter Metadata Acquisition System Enhancements

## Overview

This document describes the enhancements made to the DHT-Hunter metadata acquisition system. The system has been improved to increase the success rate of metadata acquisition for BitTorrent info hashes by implementing multiple acquisition methods, connection pooling, NAT traversal, and tracker integration.

## Table of Contents

1. [Architecture](#architecture)
2. [Components](#components)
   - [Metadata Acquisition Manager](#metadata-acquisition-manager)
   - [BitTorrent Metadata Exchange](#bittorrent-metadata-exchange)
   - [DHT Metadata Provider](#dht-metadata-provider)
   - [Connection Pool](#connection-pool)
   - [Tracker Manager](#tracker-manager)
   - [NAT Traversal Manager](#nat-traversal-manager)
3. [Implementation Details](#implementation-details)
   - [UPnP/NAT-PMP Support](#upnpnat-pmp-support)
   - [Tracker Announcements](#tracker-announcements)
   - [DHT Metadata Extensions](#dht-metadata-extensions)
   - [Connection Pooling](#connection-pooling)
4. [Usage](#usage)
5. [Limitations and Future Work](#limitations-and-future-work)

## Architecture

The enhanced metadata acquisition system uses a multi-provider approach to acquire metadata for BitTorrent info hashes. The system tries multiple methods in parallel:

1. **BitTorrent Metadata Exchange (BEP 9)**: Connects to peers from the DHT network and uses the BitTorrent extension protocol to request metadata.
2. **DHT Metadata Provider (BEP 51)**: Uses the DHT metadata extension to request metadata directly from DHT nodes.
3. **Tracker Announcements**: Announces to BitTorrent trackers to discover additional peers and then uses the BitTorrent metadata exchange protocol with those peers.

The system also includes support for NAT traversal (UPnP and NAT-PMP) to improve connectivity and a connection pool to efficiently manage peer connections.

## Components

### Metadata Acquisition Manager

The `MetadataAcquisitionManager` is the central component that coordinates the metadata acquisition process. It:

- Maintains a queue of info hashes to acquire metadata for
- Processes the queue periodically
- Coordinates the different metadata providers
- Handles success and failure cases
- Manages acquisition timeouts and retries

### BitTorrent Metadata Exchange

The `MetadataExchange` component implements the BitTorrent extension protocol (BEP 9) to acquire metadata from peers. It:

- Connects to peers
- Performs the BitTorrent handshake
- Negotiates extension protocol support
- Requests and assembles metadata pieces
- Handles connection errors and timeouts

### DHT Metadata Provider

The `DHTMetadataProvider` component implements the DHT metadata extension (BEP 51) to acquire metadata directly from DHT nodes. It:

- Sends get_metadata queries to DHT nodes
- Processes metadata responses
- Extracts torrent name, files, and sizes
- Handles errors and retries

### Connection Pool

The `ConnectionPool` component manages TCP connections to peers for efficient reuse. It:

- Maintains a pool of connections
- Provides connections to clients
- Returns connections to the pool when done
- Cleans up idle connections
- Limits the number of connections per endpoint and in total

### Tracker Manager

The `TrackerManager` component manages BitTorrent trackers and communicates with them to discover peers. It:

- Maintains a list of trackers
- Announces to trackers to discover peers
- Scrapes trackers to get statistics
- Supports both HTTP and UDP tracker protocols

### NAT Traversal Manager

The `NATTraversalManager` component manages NAT traversal to improve connectivity. It:

- Manages UPnP and NAT-PMP services
- Adds port mappings for DHT and BitTorrent
- Gets the external IP address
- Removes port mappings when shutting down

## Implementation Details

### UPnP/NAT-PMP Support

The system includes support for UPnP and NAT-PMP protocols to traverse NAT devices:

- **UPnP Service**: Implements the UPnP protocol to discover UPnP devices, select a device, and add port mappings.
- **NAT-PMP Service**: Implements the NAT-PMP protocol to discover the gateway, get the external IP address, and add port mappings.
- **NAT Traversal Manager**: Manages both UPnP and NAT-PMP services and provides a unified interface.

### Tracker Announcements

The system includes support for BitTorrent tracker protocols to discover additional peers:

- **HTTP Tracker**: Implements the HTTP tracker protocol to announce and scrape.
- **UDP Tracker**: Implements the UDP tracker protocol to announce and scrape.
- **Tracker Manager**: Manages multiple trackers and provides a unified interface.

### DHT Metadata Extensions

The DHT Metadata Provider has been enhanced to support the DHT metadata extension (BEP 51):

- Implements the get_metadata query
- Processes metadata responses
- Extracts torrent name, files, and sizes
- Handles multi-file torrents
- Implements retry mechanisms

### Connection Pooling

The Connection Pool has been implemented to efficiently manage peer connections:

- Maintains a pool of connections
- Provides connections to clients
- Returns connections to the pool when done
- Cleans up idle connections
- Limits the number of connections per endpoint and in total

## Usage

The metadata acquisition system is used by the DHT-Hunter application to acquire metadata for BitTorrent info hashes discovered by the DHT crawler. The system is initialized and started by the main application:

```cpp
// Initialize the metadata acquisition manager
auto peerStorage = std::make_shared<dht::PeerStorage>();
auto metadataAcquisitionManager = std::make_shared<bittorrent::metadata::MetadataAcquisitionManager>(peerStorage);

// Start the metadata acquisition manager
metadataAcquisitionManager->start();

// Acquire metadata for an info hash
types::InfoHash infoHash = /* ... */;
metadataAcquisitionManager->acquireMetadata(infoHash);
```

## Limitations and Future Work

The current implementation has some limitations and areas for future improvement:

### Limitations

1. **Simulated Components**: Some components are implemented with simulated functionality rather than actual network communication:
   - UPnP and NAT-PMP services simulate device discovery and port mapping
   - UDP tracker implementation simulates UDP protocol
   - DHT Metadata Provider simulates BEP 51 protocol
   - HTTP tracker implementation simulates HTTP requests and responses

2. **Connection Management**: The connection pool architecture is implemented, but the actual connection management is simulated.

3. **Error Handling**: The error handling could be improved with more sophisticated retry logic and circuit breakers.

### Future Work

1. **Implement Magnet Link Parsing**:
   - Parse magnet links to extract tracker information
   - Use tracker information to find more peers

2. **Add Support for DHT Bootstrapping**:
   - Implement a more robust DHT bootstrapping mechanism
   - Add support for more DHT bootstrap nodes

3. **Improve Error Handling and Retry Logic**:
   - Add more sophisticated retry logic with exponential backoff
   - Implement circuit breakers to avoid repeatedly trying failed resources

4. **Add Support for More Tracker Protocols**:
   - Implement WebSocket tracker protocol
   - Add support for HTTPS trackers

5. **Replace Simulated Components with Real Implementations**:
   - Implement actual UPnP and NAT-PMP network communication
   - Implement actual UDP tracker network communication
   - Implement actual DHT metadata extension network communication
   - Implement actual HTTP tracker network communication
