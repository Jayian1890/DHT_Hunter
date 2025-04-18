#include "dht_hunter/network/platform/unix/select_multiplexer.hpp"
#include "dht_hunter/logforge/logforge.hpp"

#include <algorithm>
#include <cstring>
#include <sys/time.h>
#include <unistd.h>

namespace dht_hunter::network {

namespace {
    static auto logger = dht_hunter::logforge::LogForge::getLogger("SelectMultiplexer");
}

SelectMultiplexer::SelectMultiplexer() : m_maxHandle(0) {
    FD_ZERO(&m_readSet);
    FD_ZERO(&m_writeSet);
    FD_ZERO(&m_errorSet);
}

SelectMultiplexer::~SelectMultiplexer() {
    // No specific cleanup needed
}

bool SelectMultiplexer::addSocket(Socket* socket, int events, IOEventCallback callback) {
    if (!socket || !socket->isValid()) {
        logger->error("Cannot add invalid socket to multiplexer");
        return false;
    }

    SocketHandle handle = socket->getHandle();
    
    // Check if the socket is already in the multiplexer
    if (m_sockets.find(handle) != m_sockets.end()) {
        logger->warning("Socket already in multiplexer, use modifySocket instead");
        return false;
    }

    // Add the socket to the map
    m_sockets[handle] = {events, callback};

    // Update the maximum handle value
    m_maxHandle = std::max(m_maxHandle, handle);

    logger->debug("Added socket {} to multiplexer with events {}", static_cast<int>(handle), events);
    return true;
}

bool SelectMultiplexer::modifySocket(Socket* socket, int events) {
    if (!socket || !socket->isValid()) {
        logger->error("Cannot modify invalid socket in multiplexer");
        return false;
    }

    SocketHandle handle = socket->getHandle();
    
    // Check if the socket is in the multiplexer
    auto it = m_sockets.find(handle);
    if (it == m_sockets.end()) {
        logger->warning("Socket not in multiplexer, use addSocket first");
        return false;
    }

    // Update the events
    it->second.events = events;

    logger->debug("Modified socket {} in multiplexer with events {}", static_cast<int>(handle), events);
    return true;
}

bool SelectMultiplexer::removeSocket(Socket* socket) {
    if (!socket) {
        logger->error("Cannot remove null socket from multiplexer");
        return false;
    }

    SocketHandle handle = socket->getHandle();
    
    // Check if the socket is in the multiplexer
    auto it = m_sockets.find(handle);
    if (it == m_sockets.end()) {
        logger->warning("Socket not in multiplexer");
        return false;
    }

    // Remove the socket from the map
    m_sockets.erase(it);

    // Recalculate the maximum handle value if this was the maximum
    if (handle == m_maxHandle) {
        m_maxHandle = 0;
        for (const auto& [socketHandle, _] : m_sockets) {
            m_maxHandle = std::max(m_maxHandle, socketHandle);
        }
    }

    logger->debug("Removed socket {} from multiplexer", static_cast<int>(handle));
    return true;
}

int SelectMultiplexer::wait(std::chrono::milliseconds timeout) {
    // Prepare the fd sets
    FD_ZERO(&m_readSet);
    FD_ZERO(&m_writeSet);
    FD_ZERO(&m_errorSet);

    for (const auto& [handle, info] : m_sockets) {
        if (hasEvent(info.events, IOEvent::Read)) {
            FD_SET(handle, &m_readSet);
        }
        if (hasEvent(info.events, IOEvent::Write)) {
            FD_SET(handle, &m_writeSet);
        }
        // Always monitor for errors
        FD_SET(handle, &m_errorSet);
    }

    // Prepare the timeout
    struct timeval tv;
    struct timeval* tvp = nullptr;

    if (timeout.count() >= 0) {
        tv.tv_sec = static_cast<long>(timeout.count() / 1000);
        tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
        tvp = &tv;
    }

    // Wait for events
    int result = ::select(static_cast<int>(m_maxHandle + 1), &m_readSet, &m_writeSet, &m_errorSet, tvp);

    if (result < 0) {
        logger->error("select() failed: {}", strerror(errno));
        return -1;
    }

    if (result == 0) {
        // Timeout
        logger->debug("select() timed out");
        
        // Notify sockets that are interested in timeout events
        for (const auto& [handle, info] : m_sockets) {
            if (hasEvent(info.events, IOEvent::Timeout)) {
                Socket* socket = nullptr;
                for (const auto& [s, _] : m_sockets) {
                    if (s == handle) {
                        socket = reinterpret_cast<Socket*>(s);
                        break;
                    }
                }
                
                if (socket && info.callback) {
                    info.callback(socket, IOEvent::Timeout);
                }
            }
        }
        
        return 0;
    }

    // Process the events
    for (const auto& [handle, info] : m_sockets) {
        int triggeredEvents = 0;
        
        if (FD_ISSET(handle, &m_readSet)) {
            triggeredEvents |= toInt(IOEvent::Read);
        }
        if (FD_ISSET(handle, &m_writeSet)) {
            triggeredEvents |= toInt(IOEvent::Write);
        }
        if (FD_ISSET(handle, &m_errorSet)) {
            triggeredEvents |= toInt(IOEvent::Error);
        }

        if (triggeredEvents != 0) {
            // Find the socket pointer
            Socket* socket = nullptr;
            for (const auto& [s, _] : m_sockets) {
                if (s == handle) {
                    socket = reinterpret_cast<Socket*>(s);
                    break;
                }
            }
            
            if (socket && info.callback) {
                // Call the callback for each triggered event
                if (hasEvent(triggeredEvents, IOEvent::Read) && hasEvent(info.events, IOEvent::Read)) {
                    info.callback(socket, IOEvent::Read);
                }
                if (hasEvent(triggeredEvents, IOEvent::Write) && hasEvent(info.events, IOEvent::Write)) {
                    info.callback(socket, IOEvent::Write);
                }
                if (hasEvent(triggeredEvents, IOEvent::Error)) {
                    info.callback(socket, IOEvent::Error);
                }
            }
        }
    }

    return result;
}

} // namespace dht_hunter::network
