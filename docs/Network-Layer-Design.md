# DHT-Hunter: Network Layer Design Document

## Overview

The network layer of DHT-Hunter is responsible for all network communication, providing a foundation for both DHT protocol operations and BitTorrent metadata exchange. This document outlines the design and implementation details of the network layer, which must support both UDP (for DHT) and TCP (for metadata exchange) communications in a cross-platform manner.

## Requirements

1. **Protocol Support**
   - [x] UDP socket implementation for DHT communication (BEP 5)
   - [x] TCP socket implementation for BitTorrent metadata exchange (BEP 9)
   - [x] Support for IPv4 and IPv6 addressing

2. **Cross-Platform Compatibility**
   - [x] Windows support using Winsock2 API
   - [x] Unix/Linux/macOS support using POSIX socket API
   - [x] Abstraction layer to hide platform-specific details

3. **Performance**
   - [ ] Asynchronous I/O for non-blocking operations
   - [ ] Efficient socket management for handling thousands of connections
   - [ ] Low overhead message processing

4. **Management Features**
   - [x] IP address and port management
   - [ ] Rate limiting and throttling mechanisms
   - [ ] Connection pooling and reuse
   - [ ] Timeout handling and retry logic

## Architecture

### Core Components

1. **Socket Abstraction Layer**
   - [x] `Socket` - Base abstract class for all socket types
   - [x] `UDPSocket` - Implementation for UDP communication
   - [x] `TCPSocket` - Implementation for TCP communication
   - [x] `SocketFactory` - Factory for creating platform-specific socket implementations

2. **Address Management**
   - [x] `NetworkAddress` - Abstraction for IPv4/IPv6 addresses
   - [x] `EndPoint` - Combination of address and port
   - [x] `AddressResolver` - DNS resolution and address validation

3. **I/O Management**
   - [ ] `IOMultiplexer` - Asynchronous I/O handling (select/poll/epoll/IOCP)
   - [ ] `SocketEvent` - Event notification for socket operations
   - [ ] `MessageBuffer` - Efficient buffer management for network messages

4. **Rate Control**
   - [ ] `RateLimiter` - Bandwidth management for uploads and downloads
   - [ ] `ConnectionThrottler` - Limits number of concurrent connections
   - [ ] `BurstController` - Handles traffic bursts without overwhelming the network

5. **Protocol Handlers**
   - [ ] `UDPMessageHandler` - Processes DHT protocol messages
   - [ ] `TCPMessageHandler` - Processes BitTorrent protocol messages
   - [ ] `MessageDispatcher` - Routes incoming messages to appropriate handlers

## Implementation Details

### Socket Abstraction

```cpp
// Base socket interface
class Socket {
public:
    virtual ~Socket() = default;

    virtual bool bind(const EndPoint& localEndpoint) = 0;
    virtual bool connect(const EndPoint& remoteEndpoint) = 0;
    virtual void close() = 0;

    virtual int send(const uint8_t* data, size_t length) = 0;
    virtual int receive(uint8_t* buffer, size_t maxLength) = 0;

    virtual bool setNonBlocking(bool nonBlocking) = 0;
    virtual bool setReuseAddress(bool reuse) = 0;

    virtual SocketError getLastError() const = 0;
};

// UDP socket implementation
class UDPSocket : public Socket {
public:
    // UDP-specific methods
    int sendTo(const uint8_t* data, size_t length, const EndPoint& destination);
    int receiveFrom(uint8_t* buffer, size_t maxLength, EndPoint& source);

    // Implementations of Socket interface
    // ...
};

// TCP socket implementation
class TCPSocket : public Socket {
public:
    // TCP-specific methods
    bool listen(int backlog);
    std::unique_ptr<TCPSocket> accept();

    // Implementations of Socket interface
    // ...
};
```

### Asynchronous I/O

```cpp
// Event-based I/O multiplexer
class IOMultiplexer {
public:
    enum class EventType {
        Read,
        Write,
        Error
    };

    using EventCallback = std::function<void(Socket*, EventType)>;

    virtual ~IOMultiplexer() = default;

    virtual bool addSocket(Socket* socket, EventType events, EventCallback callback) = 0;
    virtual bool modifySocket(Socket* socket, EventType events) = 0;
    virtual bool removeSocket(Socket* socket) = 0;

    virtual int wait(std::chrono::milliseconds timeout) = 0;
};

// Platform-specific implementations
class SelectMultiplexer : public IOMultiplexer {
    // Implementation using select()
};

class EpollMultiplexer : public IOMultiplexer {
    // Implementation using epoll() (Linux)
};

class KqueueMultiplexer : public IOMultiplexer {
    // Implementation using kqueue() (BSD/macOS)
};

class IOCPMultiplexer : public IOMultiplexer {
    // Implementation using I/O Completion Ports (Windows)
};
```

### Rate Limiting

```cpp
class RateLimiter {
public:
    RateLimiter(size_t bytesPerSecond);

    // Request permission to send/receive bytes
    bool request(size_t bytes);

    // Update rate limit
    void setRate(size_t bytesPerSecond);

private:
    size_t m_bytesPerSecond;
    size_t m_availableTokens;
    std::chrono::steady_clock::time_point m_lastUpdate;
    std::mutex m_mutex;

    void updateTokens();
};
```

## Bencode Implementation

The bencode implementation provides encoding and decoding of the bencode format as specified in the BitTorrent specification. This is essential for DHT protocol communication and metadata exchange.

1. **Features**
   - [x] Support for all bencode types (integers, strings, lists, dictionaries)
   - [x] Efficient parsing with minimal memory allocations
   - [x] Error handling for malformed bencode data
   - [x] Conversion between bencode and native C++ types

2. **Components**
   - [x] `BencodeValue` - Represents a bencode value (string, integer, list, or dictionary)
   - [x] `BencodeEncoder` - Encodes C++ objects to bencode format
   - [x] `BencodeDecoder` - Decodes bencode data to C++ objects
   - [x] `BencodeException` - Exception class for bencode errors

## UDP Implementation for DHT

The UDP implementation will focus on supporting the DHT protocol as specified in BEP 5. Key considerations include:

1. **Message Handling**
   - [x] Support for KRPC protocol (queries, responses, errors)
   - [x] Bencode encoding/decoding integration
   - [x] Transaction ID management

2. **Reliability**
   - [ ] Timeout and retry mechanisms for unreliable UDP
   - [ ] Duplicate message detection
   - [ ] Error handling for packet loss

3. **Performance**
   - [ ] Efficient packet processing
   - [ ] Minimizing memory allocations
   - [ ] Batching of outgoing messages when possible

## DHT Protocol Implementation

The DHT protocol implementation provides the necessary components for communicating with the BitTorrent DHT network as specified in BEP 5.

1. **Message Types**
   - [x] Query messages (ping, find_node, get_peers, announce_peer)
   - [x] Response messages for each query type
   - [x] Error messages with code and description

2. **Components**
   - [x] `Message` - Base class for all DHT messages
   - [x] `QueryMessage` - Base class for all query messages
   - [x] `ResponseMessage` - Base class for all response messages
   - [x] `ErrorMessage` - Class for error messages
   - [ ] `DHTNode` - Represents a node in the DHT network
   - [ ] `RoutingTable` - Manages known nodes in the DHT network

3. **Operations**
   - [ ] Node discovery and bootstrap
   - [ ] Finding peers for a specific infohash
   - [ ] Announcing as a peer for a specific infohash
   - [ ] Maintaining the routing table

## TCP Implementation for Metadata Exchange

The TCP implementation will support the BitTorrent protocol extensions for metadata exchange as specified in BEP 9:

1. **Connection Management**
   - [ ] Handshake process with extension protocol support
   - [ ] Connection pooling for multiple transfers
   - [ ] Graceful connection termination

2. **Metadata Transfer**
   - [ ] Requesting metadata pieces
   - [ ] Assembling complete metadata from pieces
   - [ ] Validation using infohash

3. **Extension Protocol**
   - [ ] Support for ut_metadata extension
   - [ ] Extension message parsing and generation
   - [ ] Handling of request, data, and reject messages

## Cross-Platform Considerations

### Windows-Specific

```cpp
// Windows socket initialization
class WinSockInitializer {
public:
    WinSockInitializer() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }

    ~WinSockInitializer() {
        WSACleanup();
    }
};

// Ensure initialization happens before any socket operations
static WinSockInitializer g_winSockInitializer;
```

### Unix/Linux/macOS-Specific

```cpp
// Signal handling for SIGPIPE
class SignalHandler {
public:
    SignalHandler() {
        struct sigaction sa;
        sa.sa_handler = SIG_IGN;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGPIPE, &sa, nullptr);
    }
};

// Ensure signal handling is set up
static SignalHandler g_signalHandler;
```

## Testing Strategy

1. **Unit Tests**
   - [x] Socket creation and basic operations
   - [x] Address parsing and validation
   - [ ] Buffer management
   - [ ] Rate limiting algorithms

2. **Integration Tests**
   - [ ] End-to-end UDP message exchange
   - [ ] TCP connection establishment and data transfer
   - [ ] Cross-platform compatibility

3. **Performance Tests**
   - [ ] Throughput measurement
   - [ ] Connection handling capacity
   - [ ] Memory usage under load

4. **Stress Tests**
   - [ ] Handling of network errors and timeouts
   - [ ] Recovery from connection failures
   - [ ] Behavior under high connection counts

## Implementation Phases

1. **Phase 1: Core Socket Abstraction** ✅
   - [x] Implement base Socket interface
   - [x] Create platform-specific implementations
   - [x] Develop address management components
   - [x] Basic synchronous I/O operations

2. **Phase 2: UDP Implementation** ✅
   - [x] Implement UDPSocket class
   - [x] Develop message framing for DHT protocol
   - [x] Add transaction management
   - [x] Implement basic reliability mechanisms

3. **Phase 3: Asynchronous I/O**
   - [ ] Implement IOMultiplexer interface
   - [ ] Create platform-specific multiplexer implementations
   - [ ] Convert socket operations to use async I/O
   - [ ] Add event-based callbacks

4. **Phase 4: TCP Implementation**
   - [x] Implement TCPSocket class
   - [ ] Add connection management
   - [ ] Implement BitTorrent handshake process
   - [ ] Support extension protocol

5. **Phase 5: Performance Optimization**
   - [ ] Implement rate limiting
   - [ ] Add connection throttling
   - [ ] Optimize buffer management
   - [ ] Performance profiling and tuning

## Conclusion

The network layer provides the foundation for DHT-Hunter's functionality, enabling both DHT protocol operations and BitTorrent metadata exchange. By implementing a robust, cross-platform socket abstraction with support for both UDP and TCP, the system will be able to efficiently communicate with the BitTorrent network while maintaining good performance characteristics.
