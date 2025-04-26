# Async Network Component

This directory contains the asynchronous networking component of the DHT Hunter project. The async component provides a high-level interface for asynchronous socket operations, with support for non-blocking I/O, callbacks, and event-driven programming.

## Key Classes

- `IOMultiplexer`: Base class for I/O multiplexers, which allow monitoring multiple sockets for I/O events.
- `AsyncSocket`: Base class for asynchronous sockets, with support for non-blocking I/O and callbacks.
- `AsyncTCPSocket`: Asynchronous TCP socket implementation.
- `AsyncUDPSocket`: Asynchronous UDP socket implementation.
- `AsyncSocketFactory`: Factory for creating asynchronous sockets.

## Platform-Specific Implementations

- `SelectMultiplexer`: I/O multiplexer using select() on Unix-like systems.
- `WSAMultiplexer`: I/O multiplexer using WSAEventSelect() on Windows.

## Usage Examples

### Creating an Asynchronous TCP Socket

```cpp
// Get the async socket factory
auto& factory = dht_hunter::network::AsyncSocketFactory::getInstance();

// Create an async TCP socket
auto socket = factory.createTCPSocket();

// Connect to a remote endpoint
socket->connect(dht_hunter::network::EndPoint("192.168.1.1", 8080), [](bool success) {
    if (success) {
        std::cout << "Connected successfully" << std::endl;
    } else {
        std::cout << "Connection failed" << std::endl;
    }
});

// Send data
auto buffer = std::make_shared<dht_hunter::network::Buffer>("Hello, world!", 13);
socket->send(buffer, [](bool success, int bytesSent) {
    if (success) {
        std::cout << "Sent " << bytesSent << " bytes" << std::endl;
    } else {
        std::cout << "Send failed" << std::endl;
    }
});

// Receive data
socket->receive(1024, [](bool success, std::shared_ptr<dht_hunter::network::Buffer> buffer, int bytesReceived) {
    if (success) {
        std::cout << "Received " << bytesReceived << " bytes: ";
        std::cout.write(reinterpret_cast<const char*>(buffer->data()), bytesReceived);
        std::cout << std::endl;
    } else {
        std::cout << "Receive failed" << std::endl;
    }
});

// Close the socket
socket->close([]() {
    std::cout << "Socket closed" << std::endl;
});
```

### Creating an Asynchronous UDP Socket

```cpp
// Get the async socket factory
auto& factory = dht_hunter::network::AsyncSocketFactory::getInstance();

// Create an async UDP socket
auto socket = factory.createUDPSocket();

// Bind the socket to a local address and port
auto localEndpoint = dht_hunter::network::EndPoint(dht_hunter::network::NetworkAddress::any(), 6881);
socket->getSocket()->bind(localEndpoint.getAddress(), localEndpoint.getPort());

// Send data to a specific endpoint
auto remoteEndpoint = dht_hunter::network::EndPoint("192.168.1.1", 6881);
auto buffer = std::make_shared<dht_hunter::network::Buffer>("Hello, world!", 13);
socket->sendTo(buffer, remoteEndpoint, [](bool success, int bytesSent) {
    if (success) {
        std::cout << "Sent " << bytesSent << " bytes" << std::endl;
    } else {
        std::cout << "Send failed" << std::endl;
    }
});

// Receive data from any endpoint
socket->receiveFrom(1024, [](bool success, std::shared_ptr<dht_hunter::network::Buffer> buffer, int bytesReceived, const dht_hunter::network::EndPoint& endpoint) {
    if (success) {
        std::cout << "Received " << bytesReceived << " bytes from " << endpoint.toString() << ": ";
        std::cout.write(reinterpret_cast<const char*>(buffer->data()), bytesReceived);
        std::cout << std::endl;
    } else {
        std::cout << "Receive failed" << std::endl;
    }
});

// Close the socket
socket->close([]() {
    std::cout << "Socket closed" << std::endl;
});
```

### Using the I/O Multiplexer Directly

```cpp
// Create an I/O multiplexer
auto multiplexer = dht_hunter::network::IOMultiplexer::create();

// Create a socket
auto socket = dht_hunter::network::Socket::create(dht_hunter::network::SocketType::TCP);

// Add the socket to the multiplexer
multiplexer->addSocket(socket, dht_hunter::network::IOEvent::Read | dht_hunter::network::IOEvent::Error, [](std::shared_ptr<dht_hunter::network::Socket> socket, dht_hunter::network::IOEvent events) {
    if (dht_hunter::network::hasEvent(events, dht_hunter::network::IOEvent::Read)) {
        std::cout << "Socket is readable" << std::endl;
    }
    if (dht_hunter::network::hasEvent(events, dht_hunter::network::IOEvent::Error)) {
        std::cout << "Socket has an error" << std::endl;
    }
});

// Wait for events
multiplexer->wait(std::chrono::milliseconds(1000));
```
