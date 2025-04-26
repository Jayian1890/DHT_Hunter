#include "dht_hunter/network/async/async_socket.hpp"
#include "dht_hunter/network/socket/socket_error.hpp"
#include <algorithm>

namespace dht_hunter::network {

std::shared_ptr<AsyncSocket> AsyncSocket::create(std::shared_ptr<Socket> socket, std::shared_ptr<IOMultiplexer> multiplexer) {
    if (!socket || !multiplexer) {
        return nullptr;
    }

    if (socket->getType() == SocketType::TCP) {
        return std::make_shared<AsyncTCPSocket>(socket, multiplexer);
    } else {
        return std::make_shared<AsyncUDPSocket>(socket, multiplexer);
    }
}

AsyncTCPSocket::AsyncTCPSocket(std::shared_ptr<Socket> socket, std::shared_ptr<IOMultiplexer> multiplexer)
    : m_socket(socket), m_multiplexer(multiplexer), m_connected(false), m_connecting(false), m_closing(false),
      m_currentSendOffset(0), m_sending(false), m_receiving(false) {
    
    // Set the socket to non-blocking mode
    m_socket->setNonBlocking(true);
    
    // Add the socket to the multiplexer
    m_multiplexer->addSocket(m_socket, IOEvent::Error, [this](std::shared_ptr<Socket> socket, IOEvent events) {
        handleIOEvent(events);
    });
}

AsyncTCPSocket::~AsyncTCPSocket() {
    close(nullptr);
}

void AsyncTCPSocket::connect(const EndPoint& endpoint, ConnectCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_connected || m_connecting) {
        if (callback) {
            callback(false);
        }
        return;
    }
    
    m_remoteEndpoint = endpoint;
    m_connectCallback = callback;
    m_connecting = true;
    
    // Start the connect operation
    bool result = false;
    
    // TODO: Implement connect operation
    
    if (result) {
        // Connect succeeded immediately
        m_connected = true;
        m_connecting = false;
        
        if (m_connectCallback) {
            m_connectCallback(true);
            m_connectCallback = nullptr;
        }
    } else {
        // Connect is in progress
        updateIOEvents();
    }
}

void AsyncTCPSocket::send(std::shared_ptr<Buffer> buffer, SendCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_connected || m_closing) {
        if (callback) {
            callback(false, 0);
        }
        return;
    }
    
    if (m_sending) {
        // Queue the send operation
        m_sendQueue.push(std::make_pair(buffer, callback));
        return;
    }
    
    // Start the send operation
    m_currentSendBuffer = buffer;
    m_currentSendOffset = 0;
    m_currentSendCallback = callback;
    m_sending = true;
    
    // Try to send immediately
    handleSend();
}

void AsyncTCPSocket::receive(size_t maxSize, ReceiveCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_connected || m_closing) {
        if (callback) {
            callback(false, nullptr, 0);
        }
        return;
    }
    
    if (m_receiving) {
        // Queue the receive operation
        m_receiveQueue.push(std::make_pair(maxSize, callback));
        return;
    }
    
    // Start the receive operation
    m_currentReceiveBuffer = std::make_shared<Buffer>(maxSize);
    m_currentReceiveCallback = callback;
    m_receiving = true;
    
    // Try to receive immediately
    handleReceive();
}

void AsyncTCPSocket::close(CloseCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_closing) {
        if (callback) {
            callback();
        }
        return;
    }
    
    m_closing = true;
    m_closeCallback = callback;
    
    // Remove the socket from the multiplexer
    m_multiplexer->removeSocket(m_socket);
    
    // Close the socket
    m_socket->close();
    
    // Reset state
    m_connected = false;
    m_connecting = false;
    m_sending = false;
    m_receiving = false;
    
    // Call the close callback
    if (m_closeCallback) {
        m_closeCallback();
        m_closeCallback = nullptr;
    }
}

EndPoint AsyncTCPSocket::getLocalEndpoint() const {
    return m_socket->getLocalEndpoint();
}

EndPoint AsyncTCPSocket::getRemoteEndpoint() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_remoteEndpoint;
}

bool AsyncTCPSocket::isConnected() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connected;
}

std::shared_ptr<Socket> AsyncTCPSocket::getSocket() const {
    return m_socket;
}

void AsyncTCPSocket::handleIOEvent(IOEvent events) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (hasEvent(events, IOEvent::Error)) {
        // Handle error
        if (m_connecting) {
            // Connect failed
            m_connecting = false;
            
            if (m_connectCallback) {
                m_connectCallback(false);
                m_connectCallback = nullptr;
            }
        }
        
        // Close the socket
        close(nullptr);
        return;
    }
    
    if (hasEvent(events, IOEvent::Write)) {
        if (m_connecting) {
            // Connect completed
            handleConnect();
        } else if (m_sending) {
            // Send operation can proceed
            handleSend();
        }
    }
    
    if (hasEvent(events, IOEvent::Read)) {
        if (m_receiving) {
            // Receive operation can proceed
            handleReceive();
        }
    }
    
    // Update the I/O events
    updateIOEvents();
}

void AsyncTCPSocket::handleConnect() {
    // Check if the connect operation succeeded
    int error = 0;
    socklen_t errorLen = sizeof(error);
    if (getsockopt(m_socket->getSocketDescriptor(), SOL_SOCKET, SO_ERROR, &error, &errorLen) == 0 && error == 0) {
        // Connect succeeded
        m_connected = true;
        m_connecting = false;
        
        if (m_connectCallback) {
            m_connectCallback(true);
            m_connectCallback = nullptr;
        }
    } else {
        // Connect failed
        m_connecting = false;
        
        if (m_connectCallback) {
            m_connectCallback(false);
            m_connectCallback = nullptr;
        }
    }
}

void AsyncTCPSocket::handleSend() {
    if (!m_sending || !m_currentSendBuffer) {
        return;
    }
    
    // Try to send the data
    size_t remaining = m_currentSendBuffer->size() - m_currentSendOffset;
    if (remaining == 0) {
        // Nothing to send
        m_sending = false;
        
        if (m_currentSendCallback) {
            m_currentSendCallback(true, static_cast<int>(m_currentSendBuffer->size()));
            m_currentSendCallback = nullptr;
        }
        
        // Start the next send operation
        if (!m_sendQueue.empty()) {
            auto next = m_sendQueue.front();
            m_sendQueue.pop();
            
            m_currentSendBuffer = next.first;
            m_currentSendOffset = 0;
            m_currentSendCallback = next.second;
            m_sending = true;
            
            handleSend();
        }
        
        return;
    }
    
    // Send the data
    int result = send(m_socket->getSocketDescriptor(),
                      m_currentSendBuffer->data() + m_currentSendOffset,
                      remaining,
                      0);
    
    if (result > 0) {
        // Data was sent
        m_currentSendOffset += result;
        
        if (m_currentSendOffset == m_currentSendBuffer->size()) {
            // All data was sent
            m_sending = false;
            
            if (m_currentSendCallback) {
                m_currentSendCallback(true, static_cast<int>(m_currentSendBuffer->size()));
                m_currentSendCallback = nullptr;
            }
            
            // Start the next send operation
            if (!m_sendQueue.empty()) {
                auto next = m_sendQueue.front();
                m_sendQueue.pop();
                
                m_currentSendBuffer = next.first;
                m_currentSendOffset = 0;
                m_currentSendCallback = next.second;
                m_sending = true;
                
                handleSend();
            }
        }
    } else {
        // Check for errors
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Would block, try again later
            return;
        } else {
            // Error occurred
            m_sending = false;
            
            if (m_currentSendCallback) {
                m_currentSendCallback(false, 0);
                m_currentSendCallback = nullptr;
            }
            
            // Close the socket
            close(nullptr);
        }
    }
}

void AsyncTCPSocket::handleReceive() {
    if (!m_receiving || !m_currentReceiveBuffer) {
        return;
    }
    
    // Try to receive data
    int result = recv(m_socket->getSocketDescriptor(),
                      m_currentReceiveBuffer->data(),
                      m_currentReceiveBuffer->size(),
                      0);
    
    if (result > 0) {
        // Data was received
        m_receiving = false;
        
        // Resize the buffer to the actual number of bytes received
        m_currentReceiveBuffer->resize(result);
        
        if (m_currentReceiveCallback) {
            m_currentReceiveCallback(true, m_currentReceiveBuffer, result);
            m_currentReceiveCallback = nullptr;
        }
        
        // Start the next receive operation
        if (!m_receiveQueue.empty()) {
            auto next = m_receiveQueue.front();
            m_receiveQueue.pop();
            
            m_currentReceiveBuffer = std::make_shared<Buffer>(next.first);
            m_currentReceiveCallback = next.second;
            m_receiving = true;
            
            handleReceive();
        }
    } else if (result == 0) {
        // Connection closed by peer
        m_receiving = false;
        
        if (m_currentReceiveCallback) {
            m_currentReceiveCallback(false, nullptr, 0);
            m_currentReceiveCallback = nullptr;
        }
        
        // Close the socket
        close(nullptr);
    } else {
        // Check for errors
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Would block, try again later
            return;
        } else {
            // Error occurred
            m_receiving = false;
            
            if (m_currentReceiveCallback) {
                m_currentReceiveCallback(false, nullptr, 0);
                m_currentReceiveCallback = nullptr;
            }
            
            // Close the socket
            close(nullptr);
        }
    }
}

void AsyncTCPSocket::updateIOEvents() {
    IOEvent events = IOEvent::Error;
    
    if (m_connecting || m_sending) {
        events = events | IOEvent::Write;
    }
    
    if (m_receiving) {
        events = events | IOEvent::Read;
    }
    
    m_multiplexer->modifySocket(m_socket, events);
}

AsyncUDPSocket::AsyncUDPSocket(std::shared_ptr<Socket> socket, std::shared_ptr<IOMultiplexer> multiplexer)
    : m_socket(socket), m_multiplexer(multiplexer), m_connected(false), m_closing(false),
      m_sending(false), m_receiving(false) {
    
    // Set the socket to non-blocking mode
    m_socket->setNonBlocking(true);
    
    // Add the socket to the multiplexer
    m_multiplexer->addSocket(m_socket, IOEvent::Error, [this](std::shared_ptr<Socket> socket, IOEvent events) {
        handleIOEvent(events);
    });
}

AsyncUDPSocket::~AsyncUDPSocket() {
    close(nullptr);
}

void AsyncUDPSocket::connect(const EndPoint& endpoint, ConnectCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_connected || m_closing) {
        if (callback) {
            callback(false);
        }
        return;
    }
    
    m_remoteEndpoint = endpoint;
    m_connected = true;
    
    if (callback) {
        callback(true);
    }
}

void AsyncUDPSocket::send(std::shared_ptr<Buffer> buffer, SendCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_connected || m_closing) {
        if (callback) {
            callback(false, 0);
        }
        return;
    }
    
    // Send to the connected endpoint
    sendTo(buffer, m_remoteEndpoint, [callback](bool success, int bytesSent) {
        if (callback) {
            callback(success, bytesSent);
        }
    });
}

void AsyncUDPSocket::receive(size_t maxSize, ReceiveCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_connected || m_closing) {
        if (callback) {
            callback(false, nullptr, 0);
        }
        return;
    }
    
    // Receive from the connected endpoint
    receiveFrom(maxSize, [this, callback](bool success, std::shared_ptr<Buffer> buffer, int bytesReceived, const EndPoint& endpoint) {
        if (callback) {
            if (success && endpoint == m_remoteEndpoint) {
                callback(true, buffer, bytesReceived);
            } else {
                callback(false, nullptr, 0);
            }
        }
    });
}

void AsyncUDPSocket::close(CloseCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_closing) {
        if (callback) {
            callback();
        }
        return;
    }
    
    m_closing = true;
    m_closeCallback = callback;
    
    // Remove the socket from the multiplexer
    m_multiplexer->removeSocket(m_socket);
    
    // Close the socket
    m_socket->close();
    
    // Reset state
    m_connected = false;
    m_sending = false;
    m_receiving = false;
    
    // Call the close callback
    if (m_closeCallback) {
        m_closeCallback();
        m_closeCallback = nullptr;
    }
}

EndPoint AsyncUDPSocket::getLocalEndpoint() const {
    return m_socket->getLocalEndpoint();
}

EndPoint AsyncUDPSocket::getRemoteEndpoint() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_remoteEndpoint;
}

bool AsyncUDPSocket::isConnected() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connected;
}

std::shared_ptr<Socket> AsyncUDPSocket::getSocket() const {
    return m_socket;
}

void AsyncUDPSocket::sendTo(std::shared_ptr<Buffer> buffer, const EndPoint& endpoint, SendToCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_closing) {
        if (callback) {
            callback(false, 0);
        }
        return;
    }
    
    if (m_sending) {
        // Queue the send operation
        m_sendToQueue.push(std::make_tuple(buffer, endpoint, callback));
        return;
    }
    
    // Start the send operation
    m_currentSendBuffer = buffer;
    m_currentSendEndpoint = endpoint;
    m_currentSendToCallback = callback;
    m_sending = true;
    
    // Try to send immediately
    handleSendTo();
    
    // Update the I/O events
    updateIOEvents();
}

void AsyncUDPSocket::receiveFrom(size_t maxSize, ReceiveFromCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_closing) {
        if (callback) {
            callback(false, nullptr, 0, EndPoint());
        }
        return;
    }
    
    if (m_receiving) {
        // Queue the receive operation
        m_receiveFromQueue.push(std::make_pair(maxSize, callback));
        return;
    }
    
    // Start the receive operation
    m_currentReceiveBuffer = std::make_shared<Buffer>(maxSize);
    m_currentReceiveFromCallback = callback;
    m_receiving = true;
    
    // Try to receive immediately
    handleReceiveFrom();
    
    // Update the I/O events
    updateIOEvents();
}

void AsyncUDPSocket::handleIOEvent(IOEvent events) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (hasEvent(events, IOEvent::Error)) {
        // Handle error
        close(nullptr);
        return;
    }
    
    if (hasEvent(events, IOEvent::Write) && m_sending) {
        // Send operation can proceed
        handleSendTo();
    }
    
    if (hasEvent(events, IOEvent::Read) && m_receiving) {
        // Receive operation can proceed
        handleReceiveFrom();
    }
    
    // Update the I/O events
    updateIOEvents();
}

void AsyncUDPSocket::handleSendTo() {
    if (!m_sending || !m_currentSendBuffer) {
        return;
    }
    
    // Try to send the data
    int result = m_socket->sendTo(m_currentSendBuffer->data(),
                                 static_cast<int>(m_currentSendBuffer->size()),
                                 m_currentSendEndpoint);
    
    if (result > 0) {
        // Data was sent
        m_sending = false;
        
        if (m_currentSendToCallback) {
            m_currentSendToCallback(true, result);
            m_currentSendToCallback = nullptr;
        }
        
        // Start the next send operation
        if (!m_sendToQueue.empty()) {
            auto next = m_sendToQueue.front();
            m_sendToQueue.pop();
            
            m_currentSendBuffer = std::get<0>(next);
            m_currentSendEndpoint = std::get<1>(next);
            m_currentSendToCallback = std::get<2>(next);
            m_sending = true;
            
            handleSendTo();
        }
    } else {
        // Check for errors
        if (m_socket->getLastError() == SocketError::WouldBlock) {
            // Would block, try again later
            return;
        } else {
            // Error occurred
            m_sending = false;
            
            if (m_currentSendToCallback) {
                m_currentSendToCallback(false, 0);
                m_currentSendToCallback = nullptr;
            }
        }
    }
}

void AsyncUDPSocket::handleReceiveFrom() {
    if (!m_receiving || !m_currentReceiveBuffer) {
        return;
    }
    
    // Try to receive data
    EndPoint source;
    int result = m_socket->receiveFrom(m_currentReceiveBuffer->data(),
                                      static_cast<int>(m_currentReceiveBuffer->size()),
                                      source);
    
    if (result > 0) {
        // Data was received
        m_receiving = false;
        
        // Resize the buffer to the actual number of bytes received
        m_currentReceiveBuffer->resize(result);
        
        if (m_currentReceiveFromCallback) {
            m_currentReceiveFromCallback(true, m_currentReceiveBuffer, result, source);
            m_currentReceiveFromCallback = nullptr;
        }
        
        // Start the next receive operation
        if (!m_receiveFromQueue.empty()) {
            auto next = m_receiveFromQueue.front();
            m_receiveFromQueue.pop();
            
            m_currentReceiveBuffer = std::make_shared<Buffer>(next.first);
            m_currentReceiveFromCallback = next.second;
            m_receiving = true;
            
            handleReceiveFrom();
        }
    } else {
        // Check for errors
        if (m_socket->getLastError() == SocketError::WouldBlock) {
            // Would block, try again later
            return;
        } else {
            // Error occurred
            m_receiving = false;
            
            if (m_currentReceiveFromCallback) {
                m_currentReceiveFromCallback(false, nullptr, 0, EndPoint());
                m_currentReceiveFromCallback = nullptr;
            }
        }
    }
}

void AsyncUDPSocket::updateIOEvents() {
    IOEvent events = IOEvent::Error;
    
    if (m_sending) {
        events = events | IOEvent::Write;
    }
    
    if (m_receiving) {
        events = events | IOEvent::Read;
    }
    
    m_multiplexer->modifySocket(m_socket, events);
}

} // namespace dht_hunter::network
