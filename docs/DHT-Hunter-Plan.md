# DHT-Hunter: DHT Node and Metadata Crawler Implementation Plan

## Project Overview
DHT-Hunter will be a cross-platform terminal-based application that functions as a real DHT node and crawls/scrapes the DHT network for active torrent metadata. The implementation will be done without relying on third-party or external libraries.

## Core Components

### 1. Logging System
- [x] Custom logging framework implementation (LogForge)
- [x] Multiple log levels (TRACE, DEBUG, INFO, WARNING, ERROR, CRITICAL)
- [x] Configurable log outputs (console, file, both)
- [x] Log rotation and management
- [x] Thread-safe logging implementation
- [x] Performance-optimized logging with minimal overhead
- [x] Context-aware logging (component, function, line number)
- [x] Custom string formatter with placeholder support

### 2. Network Layer
- [x] UDP socket implementation for DHT communication
- [x] TCP socket implementation for metadata exchange
- [x] Cross-platform socket handling (Windows/Unix compatibility)
- [ ] Asynchronous I/O handling for network operations
- [x] IP address and port management
- [ ] Rate limiting and throttling mechanisms
- [ ] Connection pooling and management

### 3. DHT Protocol Implementation (Kademlia)
- [x] Node ID generation and management
- [x] Routing table implementation (k-buckets)
- [x] XOR-based distance metric for node proximity
- [ ] Node lookup algorithm
- [ ] Key/value pair storage
- [x] DHT message handling (ping, find_node, get_peers, announce_peer)
- [x] KRPC protocol implementation (RPC over UDP using bencode)

### 4. Bencode Implementation
- [x] Bencode encoder (for outgoing messages)
- [x] Bencode decoder (for incoming messages)
- [x] Support for all bencode types (integers, strings, lists, dictionaries)

### 5. BitTorrent Metadata Exchange
- [ ] Implementation of BEP 9 (Extension for Peers to Send Metadata Files)
- [ ] TCP connection handling for peer communication
- [ ] BitTorrent protocol handshake with extension support
- [ ] Metadata piece handling and assembly
- [ ] Info dictionary parsing
- [ ] Torrent file construction from metadata

### 6. DHT Crawler/Scraper
- [ ] Bootstrap node discovery
- [ ] Aggressive node discovery algorithm
- [ ] Infohash collection from get_peers responses
- [ ] Metadata retrieval for discovered infohashes
- [ ] Efficient storage of discovered metadata

### 7. Terminal UI
- [ ] Cross-platform terminal interface
- [ ] Real-time statistics display
- [ ] Interactive commands
- [ ] Configuration options

### 8. Data Storage
- [ ] Efficient storage format for collected metadata
- [ ] Persistence mechanism for routing table
- [ ] Export functionality for discovered torrents

## Implementation Phases

### Phase 1: Project Setup and Logging System
- [x] Set up project structure
- [x] Implement comprehensive logging system (LogForge)
- [x] Create logging configuration system
- [x] Implement log rotation and management
- [x] Add thread-safe logging capabilities
- [x] Create unit tests for logging system
- [x] Implement asynchronous logging with background thread
- [x] Create custom string formatter with placeholder support

### Phase 2: Core Network and Protocol Implementation
- [x] Implement socket abstraction layer for cross-platform compatibility
- [x] Implement basic UDP socket handling for DHT
- [x] Implement basic TCP socket handling for metadata exchange
- [ ] Implement asynchronous I/O multiplexer
- [x] Implement bencode encoder/decoder
- [x] Implement basic KRPC message handling
- [x] Create routing table structure
- [x] Implement basic DHT node functionality (ping, find_node)

### Phase 3: DHT Node Functionality
- [x] Complete DHT protocol message types
- [ ] Implement DHT node operations
- [ ] Implement get_peers and announce_peer functionality
- [ ] Add bootstrap mechanism
- [ ] Implement node lookup algorithm
- [ ] Create persistent routing table
- [ ] Test basic DHT node functionality

### Phase 4: Metadata Exchange and Crawling
- [ ] Implement BEP 9 metadata exchange
- [ ] Implement BitTorrent protocol handshake with extension support
- [ ] Implement TCP connection management for peer communication
- [ ] Create infohash collection mechanism
- [ ] Implement metadata retrieval from peers
- [ ] Add storage for collected metadata
- [ ] Optimize crawling algorithm

### Phase 5: Terminal UI and Finalization
- [ ] Implement cross-platform terminal UI
- [ ] Add configuration options
- [ ] Create statistics tracking
- [x] Enhance logging system with advanced features (color support)
- [ ] Add export functionality
- [ ] Performance optimization
- [ ] Final testing and debugging

## Technical Details

### DHT Protocol (BEP 5)
The DHT implementation will follow the Kademlia algorithm as specified in BEP 5:
- 160-bit node IDs (matching SHA-1 infohashes)
- XOR metric for determining node distance
- k-buckets for routing table organization (k=8 typically)
- Four main RPC methods: ping, find_node, get_peers, announce_peer

### KRPC Protocol
Messages will be encoded using bencode and sent over UDP:
- Query messages (with transaction ID, "q" type, method name, and arguments)
- Response messages (with transaction ID, "r" type, and response values)
- Error messages (with transaction ID, "e" type, and error details)

### Metadata Exchange (BEP 9)
For retrieving torrent metadata:
- Connect to peers using TCP and standard BitTorrent handshake
- Implement extension protocol handshake
- Request metadata pieces using ut_metadata extension
- Assemble pieces into complete metadata
- Validate metadata against infohash

### Crawler Optimization
- Maintain a queue of nodes to contact
- Prioritize nodes based on activity and response rate
- Implement parallel requests to maximize discovery
- Use bloom filters to avoid duplicate processing

### Logging System (LogForge)
- [x] Custom implementation without external dependencies
- [x] Hierarchical logger structure with inheritance of log levels
- [x] Compile-time filtering for zero-overhead in release builds
- [ ] Structured logging with JSON output option for machine parsing
- [x] Contextual logging with thread ID, timestamp, component, and severity
- [x] Asynchronous logging with dedicated background thread for minimal performance impact
- [x] Configurable log sinks (console, file, memory buffer)
- [x] Log rotation based on size or time intervals
- [x] Color-coded console output for different log levels
- [x] Custom string formatter with {} placeholders and positional arguments
- [x] Thread-safe implementation with mutex protection
- [x] Comprehensive unit tests and example applications

## Project Structure

The project will follow modern C++ project organization practices with a clean, modular directory structure using CMake as the build system. The structure will be as follows:

```
DHT-Hunter/
├── CMakeLists.txt              # Root CMake configuration file
├── .gitignore                  # Git ignore file
├── README.md                   # Project documentation
├── LICENSE                     # License file
├── cmake/                      # CMake modules and utilities
│   └── ...
├── include/                    # Public header files
│   └── dht_hunter/             # Project namespace directory
│       ├── logging/            # Logging system headers
│       ├── network/            # Network layer headers
│       ├── dht/                # DHT protocol headers
│       ├── bencode/            # Bencode implementation headers
│       ├── metadata/           # Metadata exchange headers
│       ├── crawler/            # Crawler/scraper headers
│       ├── ui/                 # Terminal UI headers
│       └── storage/            # Data storage headers
├── src/                        # Implementation files
│   ├── logging/                # Logging system implementation
│   ├── network/                # Network layer implementation
│   ├── dht/                    # DHT protocol implementation
│   ├── bencode/                # Bencode implementation
│   ├── metadata/               # Metadata exchange implementation
│   ├── crawler/                # Crawler/scraper implementation
│   ├── ui/                     # Terminal UI implementation
│   ├── storage/                # Data storage implementation
│   └── main.cpp                # Application entry point
├── tests/                      # Test directory
│   ├── CMakeLists.txt          # Test CMake configuration
│   ├── unit/                   # Unit tests
│   │   ├── logging/            # Logging system tests
│   │   ├── network/            # Network layer tests
│   │   ├── dht/                # DHT protocol tests
│   │   ├── bencode/            # Bencode tests
│   │   └── ...                 # Other component tests
│   └── integration/            # Integration tests
├── examples/                   # Example code and usage demos
│   ├── CMakeLists.txt          # Examples CMake configuration
│   ├── logging_demo/           # Logging system usage example
│   ├── simple_node/            # Simple DHT node example
│   └── metadata_fetch/         # Metadata fetching example
└── docs/                       # Documentation
    ├── design/                 # Design documents
    └── api/                    # API documentation
```

### Key Structure Principles

1. **Separation of Interface and Implementation**: Public headers in `include/` and implementation in `src/`
2. **Modular Organization**: Code organized by functional components
3. **Namespace Consistency**: All headers under project namespace directory
4. **Test-Driven Development**: Comprehensive test structure mirroring source organization
5. **Modern CMake Practices**: Target-based approach with proper dependency management
6. **Documentation**: Dedicated space for design documents and API documentation

### Build System

The project will use CMake with the following features:
- Minimum CMake version 3.14 or higher
- Target-based approach (using `target_include_directories`, `target_link_libraries`, etc.)
- Proper dependency management
- Compiler warnings and static analysis integration
- Cross-platform compatibility settings
- Testing framework integration (likely Google Test or Catch2)

## Resources and References
- BEP 5: DHT Protocol
- BEP 9: Extension for Peers to Send Metadata Files
- BEP 10: Extension Protocol
- Kademlia: A Peer-to-peer Information System Based on the XOR Metric
- BitTorrent Protocol Specification (BEP 3)
- Modern CMake practices
- C++ Core Guidelines
