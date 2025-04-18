#include "dht_hunter/network/async_socket.hpp"
#include "dht_hunter/logforge/logforge.hpp"

namespace dht_hunter::network {

namespace {
    static auto logger = dht_hunter::logforge::LogForge::getLogger("AsyncSocket");
}

AsyncSocket::AsyncSocket(std::unique_ptr<Socket> socket, std::shared_ptr<IOMultiplexer> multiplexer)
    : m_socket(std::move(socket)), m_multiplexer(multiplexer),
      m_readPending(false), m_writePending(false),
      m_acceptPending(false), m_connectPending(false) {
    
    // Set the socket to non-blocking mode
    m_socket->setNonBlocking(true);
}

AsyncSocket::~AsyncSocket() {
    // Cancel all pending operations
    cancelAll();
    
    // Remove the socket from the multiplexer
    m_multiplexer->removeSocket(m_socket.get());
}

Socket* AsyncSocket::getSocket() const {
    return m_socket.get();
}

bool AsyncSocket::asyncRead(uint8_t* buffer, int size, AsyncReadCallback callback) {
    if (!m_socket || !m_socket->isValid()) {
        logger->error("Cannot read from invalid socket");
        return false;
    }
    
    if (m_readPending) {
        logger->warning("Read operation already pending");
        return false;
    }
    
    // Store the read operation
    m_pendingRead = {buffer, size, callback};
    m_readPending = true;
    
    // Add the socket to the multiplexer for read events
    int events = toInt(IOEvent::Read);
    if (m_writePending) {
        events |= toInt(IOEvent::Write);
    }
    
    if (m_multiplexer->addSocket(m_socket.get(), events, 
                                [this](Socket* socket, IOEvent event) {
                                    this->handleEvent(socket, event);
                                })) {
        logger->debug("Added socket to multiplexer for read events");
        return true;
    } else if (m_multiplexer->modifySocket(m_socket.get(), events)) {
        logger->debug("Modified socket in multiplexer for read events");
        return true;
    } else {
        logger->error("Failed to add/modify socket in multiplexer for read events");
        m_readPending = false;
        return false;
    }
}

bool AsyncSocket::asyncWrite(const uint8_t* data, int size, AsyncWriteCallback callback) {
    if (!m_socket || !m_socket->isValid()) {
        logger->error("Cannot write to invalid socket");
        return false;
    }
    
    if (m_writePending) {
        logger->warning("Write operation already pending");
        return false;
    }
    
    // Store the write operation
    m_pendingWrite.data.assign(data, data + size);
    m_pendingWrite.offset = 0;
    m_pendingWrite.callback = callback;
    m_writePending = true;
    
    // Add the socket to the multiplexer for write events
    int events = toInt(IOEvent::Write);
    if (m_readPending) {
        events |= toInt(IOEvent::Read);
    }
    
    if (m_multiplexer->addSocket(m_socket.get(), events, 
                                [this](Socket* socket, IOEvent event) {
                                    this->handleEvent(socket, event);
                                })) {
        logger->debug("Added socket to multiplexer for write events");
        return true;
    } else if (m_multiplexer->modifySocket(m_socket.get(), events)) {
        logger->debug("Modified socket in multiplexer for write events");
        return true;
    } else {
        logger->error("Failed to add/modify socket in multiplexer for write events");
        m_writePending = false;
        return false;
    }
}

bool AsyncSocket::asyncAccept(AsyncAcceptCallback callback) {
    if (!m_socket || !m_socket->isValid()) {
        logger->error("Cannot accept on invalid socket");
        return false;
    }
    
    if (m_acceptPending) {
        logger->warning("Accept operation already pending");
        return false;
    }
    
    // Store the accept callback
    m_acceptCallback = callback;
    m_acceptPending = true;
    
    // Add the socket to the multiplexer for read events (accept is a read event)
    int events = toInt(IOEvent::Read);
    
    if (m_multiplexer->addSocket(m_socket.get(), events, 
                                [this](Socket* socket, IOEvent event) {
                                    this->handleEvent(socket, event);
                                })) {
        logger->debug("Added socket to multiplexer for accept events");
        return true;
    } else if (m_multiplexer->modifySocket(m_socket.get(), events)) {
        logger->debug("Modified socket in multiplexer for accept events");
        return true;
    } else {
        logger->error("Failed to add/modify socket in multiplexer for accept events");
        m_acceptPending = false;
        return false;
    }
}

bool AsyncSocket::asyncConnect(const EndPoint& endpoint, AsyncConnectCallback callback) {
    if (!m_socket || !m_socket->isValid()) {
        logger->error("Cannot connect with invalid socket");
        return false;
    }
    
    if (m_connectPending) {
        logger->warning("Connect operation already pending");
        return false;
    }
    
    // Store the connect callback
    m_connectCallback = callback;
    m_connectPending = true;
    
    // Start the connect operation
    if (!m_socket->connect(endpoint)) {
        SocketError error = m_socket->getLastError();
        
        // If the error is WouldBlock, the connect is in progress
        if (error != SocketError::WouldBlock) {
            logger->error("Connect failed: {}", Socket::getErrorString(error));
            m_connectPending = false;
            return false;
        }
    }
    
    // Add the socket to the multiplexer for write events (connect completion is a write event)
    int events = toInt(IOEvent::Write);
    
    if (m_multiplexer->addSocket(m_socket.get(), events, 
                                [this](Socket* socket, IOEvent event) {
                                    this->handleEvent(socket, event);
                                })) {
        logger->debug("Added socket to multiplexer for connect events");
        return true;
    } else if (m_multiplexer->modifySocket(m_socket.get(), events)) {
        logger->debug("Modified socket in multiplexer for connect events");
        return true;
    } else {
        logger->error("Failed to add/modify socket in multiplexer for connect events");
        m_connectPending = false;
        return false;
    }
}

void AsyncSocket::cancelAll() {
    // Cancel all pending operations
    if (m_readPending && m_pendingRead.callback) {
        m_pendingRead.callback(m_socket.get(), nullptr, 0, SocketError::Cancelled);
        m_readPending = false;
    }
    
    if (m_writePending && m_pendingWrite.callback) {
        m_pendingWrite.callback(m_socket.get(), 0, SocketError::Cancelled);
        m_writePending = false;
    }
    
    if (m_acceptPending && m_acceptCallback) {
        m_acceptCallback(m_socket.get(), nullptr, SocketError::Cancelled);
        m_acceptPending = false;
    }
    
    if (m_connectPending && m_connectCallback) {
        m_connectCallback(m_socket.get(), SocketError::Cancelled);
        m_connectPending = false;
    }
}

void AsyncSocket::handleEvent(Socket* socket, IOEvent event) {
    if (socket != m_socket.get()) {
        logger->error("Event for wrong socket");
        return;
    }
    
    if (event == IOEvent::Error) {
        // Handle error event
        SocketError error = m_socket->getLastError();
        logger->error("Socket error: {}", Socket::getErrorString(error));
        
        // Notify all pending operations
        if (m_readPending && m_pendingRead.callback) {
            m_pendingRead.callback(m_socket.get(), nullptr, 0, error);
            m_readPending = false;
        }
        
        if (m_writePending && m_pendingWrite.callback) {
            m_pendingWrite.callback(m_socket.get(), 0, error);
            m_writePending = false;
        }
        
        if (m_acceptPending && m_acceptCallback) {
            m_acceptCallback(m_socket.get(), nullptr, error);
            m_acceptPending = false;
        }
        
        if (m_connectPending && m_connectCallback) {
            m_connectCallback(m_socket.get(), error);
            m_connectPending = false;
        }
        
        return;
    }
    
    if (event == IOEvent::Read) {
        // Handle read event
        if (m_acceptPending) {
            // Accept a new connection
            EndPoint clientEndpoint;
            std::unique_ptr<Socket> clientSocket = m_socket->accept(clientEndpoint);
            
            if (clientSocket) {
                logger->debug("Accepted connection from {}", clientEndpoint.toString());
                
                if (m_acceptCallback) {
                    m_acceptCallback(m_socket.get(), std::move(clientSocket), SocketError::None);
                }
            } else {
                SocketError error = m_socket->getLastError();
                
                if (error == SocketError::WouldBlock) {
                    // No connection to accept, keep waiting
                    return;
                }
                
                logger->error("Accept failed: {}", Socket::getErrorString(error));
                
                if (m_acceptCallback) {
                    m_acceptCallback(m_socket.get(), nullptr, error);
                }
            }
            
            m_acceptPending = false;
        } else if (m_readPending) {
            // Read data from the socket
            int bytesRead = m_socket->receive(m_pendingRead.buffer, m_pendingRead.size);
            
            if (bytesRead > 0) {
                logger->debug("Read {} bytes", bytesRead);
                
                if (m_pendingRead.callback) {
                    m_pendingRead.callback(m_socket.get(), m_pendingRead.buffer, bytesRead, SocketError::None);
                }
                
                m_readPending = false;
            } else if (bytesRead == 0) {
                // Connection closed
                logger->debug("Connection closed");
                
                if (m_pendingRead.callback) {
                    m_pendingRead.callback(m_socket.get(), nullptr, 0, SocketError::ConnectionClosed);
                }
                
                m_readPending = false;
            } else {
                SocketError error = m_socket->getLastError();
                
                if (error == SocketError::WouldBlock) {
                    // No data available, keep waiting
                    return;
                }
                
                logger->error("Read failed: {}", Socket::getErrorString(error));
                
                if (m_pendingRead.callback) {
                    m_pendingRead.callback(m_socket.get(), nullptr, 0, error);
                }
                
                m_readPending = false;
            }
        }
    }
    
    if (event == IOEvent::Write) {
        // Handle write event
        if (m_connectPending) {
            // Check if the connect completed
            SocketError error = m_socket->getLastError();
            
            if (error == SocketError::None) {
                logger->debug("Connect completed");
                
                if (m_connectCallback) {
                    m_connectCallback(m_socket.get(), SocketError::None);
                }
            } else {
                logger->error("Connect failed: {}", Socket::getErrorString(error));
                
                if (m_connectCallback) {
                    m_connectCallback(m_socket.get(), error);
                }
            }
            
            m_connectPending = false;
        } else if (m_writePending) {
            // Write data to the socket
            int bytesWritten = m_socket->send(m_pendingWrite.data.data() + m_pendingWrite.offset, 
                                            static_cast<int>(m_pendingWrite.data.size()) - m_pendingWrite.offset);
            
            if (bytesWritten > 0) {
                logger->debug("Wrote {} bytes", bytesWritten);
                
                m_pendingWrite.offset += bytesWritten;
                
                if (m_pendingWrite.offset >= static_cast<int>(m_pendingWrite.data.size())) {
                    // All data written
                    if (m_pendingWrite.callback) {
                        m_pendingWrite.callback(m_socket.get(), m_pendingWrite.offset, SocketError::None);
                    }
                    
                    m_writePending = false;
                } else {
                    // More data to write, keep waiting
                    return;
                }
            } else {
                SocketError error = m_socket->getLastError();
                
                if (error == SocketError::WouldBlock) {
                    // Socket not ready for writing, keep waiting
                    return;
                }
                
                logger->error("Write failed: {}", Socket::getErrorString(error));
                
                if (m_pendingWrite.callback) {
                    m_pendingWrite.callback(m_socket.get(), m_pendingWrite.offset, error);
                }
                
                m_writePending = false;
            }
        }
    }
    
    // Update the events we're interested in
    int events = 0;
    
    if (m_readPending || m_acceptPending) {
        events |= toInt(IOEvent::Read);
    }
    
    if (m_writePending || m_connectPending) {
        events |= toInt(IOEvent::Write);
    }
    
    if (events != 0) {
        m_multiplexer->modifySocket(m_socket.get(), events);
    } else {
        m_multiplexer->removeSocket(m_socket.get());
    }
}

} // namespace dht_hunter::network
