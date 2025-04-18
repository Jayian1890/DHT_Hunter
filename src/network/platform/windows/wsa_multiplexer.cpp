#include "dht_hunter/network/platform/windows/wsa_multiplexer.hpp"
#include "dht_hunter/logforge/logforge.hpp"

#ifdef _WIN32

namespace dht_hunter::network {

namespace {
    static auto logger = dht_hunter::logforge::LogForge::getLogger("WSAMultiplexer");
}

WSAMultiplexer::WSAMultiplexer() {
    // Initialize the vectors
    m_events.reserve(WSA_MAXIMUM_WAIT_EVENTS);
    m_handles.reserve(WSA_MAXIMUM_WAIT_EVENTS);
}

WSAMultiplexer::~WSAMultiplexer() {
    // Clean up all event handles
    for (const auto& [_, info] : m_sockets) {
        WSACloseEvent(info.wsaEvent);
    }
}

bool WSAMultiplexer::addSocket(Socket* socket, int events, IOEventCallback callback) {
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

    // Check if we have reached the maximum number of sockets
    if (m_sockets.size() >= WSA_MAXIMUM_WAIT_EVENTS) {
        logger->error("Cannot add more sockets to multiplexer, maximum reached");
        return false;
    }

    // Create a new event handle
    WSAEVENT wsaEvent = WSACreateEvent();
    if (wsaEvent == WSA_INVALID_EVENT) {
        logger->error("Failed to create WSA event: {}", WSAGetLastError());
        return false;
    }

    // Associate the event with the socket
    long wsaEvents = toWSAEvents(events);
    if (WSAEventSelect(static_cast<SOCKET>(handle), wsaEvent, wsaEvents) == SOCKET_ERROR) {
        logger->error("Failed to associate event with socket: {}", WSAGetLastError());
        WSACloseEvent(wsaEvent);
        return false;
    }

    // Add the socket to the map
    m_sockets[handle] = {events, callback, wsaEvent};

    // Update the vectors
    m_events.push_back(wsaEvent);
    m_handles.push_back(handle);

    logger->debug("Added socket {} to multiplexer with events {}", static_cast<int>(handle), events);
    return true;
}

bool WSAMultiplexer::modifySocket(Socket* socket, int events) {
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

    // Associate the event with the socket
    long wsaEvents = toWSAEvents(events);
    if (WSAEventSelect(static_cast<SOCKET>(handle), it->second.wsaEvent, wsaEvents) == SOCKET_ERROR) {
        logger->error("Failed to associate event with socket: {}", WSAGetLastError());
        return false;
    }

    // Update the events
    it->second.events = events;

    logger->debug("Modified socket {} in multiplexer with events {}", static_cast<int>(handle), events);
    return true;
}

bool WSAMultiplexer::removeSocket(Socket* socket) {
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

    // Close the event handle
    WSACloseEvent(it->second.wsaEvent);

    // Remove the socket from the vectors
    for (size_t i = 0; i < m_handles.size(); ++i) {
        if (m_handles[i] == handle) {
            m_events.erase(m_events.begin() + i);
            m_handles.erase(m_handles.begin() + i);
            break;
        }
    }

    // Remove the socket from the map
    m_sockets.erase(it);

    logger->debug("Removed socket {} from multiplexer", static_cast<int>(handle));
    return true;
}

int WSAMultiplexer::wait(std::chrono::milliseconds timeout) {
    if (m_events.empty()) {
        logger->warning("No sockets in multiplexer");
        return 0;
    }

    // Wait for events
    DWORD waitTime = (timeout.count() < 0) ? WSA_INFINITE : static_cast<DWORD>(timeout.count());
    DWORD result = WSAWaitForMultipleEvents(static_cast<DWORD>(m_events.size()), m_events.data(), FALSE, waitTime, FALSE);

    if (result == WSA_WAIT_FAILED) {
        logger->error("WSAWaitForMultipleEvents failed: {}", WSAGetLastError());
        return -1;
    }

    if (result == WSA_WAIT_TIMEOUT) {
        // Timeout
        logger->debug("WSAWaitForMultipleEvents timed out");

        // Notify sockets that are interested in timeout events
        for (const auto& [handle, info] : m_sockets) {
            if (hasEvent(info.events, IOEvent::Timeout)) {
                Socket* socket = reinterpret_cast<Socket*>(handle);
                if (socket && info.callback) {
                    info.callback(socket, IOEvent::Timeout);
                }
            }
        }

        return 0;
    }

    // Calculate the index of the event that was signaled
    DWORD eventIndex = result - WSA_WAIT_EVENT_0;
    if (eventIndex >= m_events.size()) {
        logger->error("Invalid event index: {}", eventIndex);
        return -1;
    }

    // Get the socket handle and info
    SocketHandle handle = m_handles[eventIndex];
    const auto& info = m_sockets[handle];

    // Reset the event
    WSAResetEvent(info.wsaEvent);

    // Get the network events
    WSANETWORKEVENTS networkEvents;
    if (WSAEnumNetworkEvents(static_cast<SOCKET>(handle), info.wsaEvent, &networkEvents) == SOCKET_ERROR) {
        logger->error("WSAEnumNetworkEvents failed: {}", WSAGetLastError());
        return -1;
    }

    // Process the events
    int triggeredEvents = 0;

    if (networkEvents.lNetworkEvents & FD_READ) {
        triggeredEvents |= toInt(IOEvent::Read);
    }
    if (networkEvents.lNetworkEvents & FD_WRITE) {
        triggeredEvents |= toInt(IOEvent::Write);
    }
    if (networkEvents.lNetworkEvents & (FD_CLOSE | FD_CONNECT)) {
        triggeredEvents |= toInt(IOEvent::Error);
    }

    if (triggeredEvents != 0) {
        Socket* socket = reinterpret_cast<Socket*>(handle);
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

    return 1;
}

long WSAMultiplexer::toWSAEvents(int events) const {
    long wsaEvents = 0;

    if (hasEvent(events, IOEvent::Read)) {
        wsaEvents |= FD_READ;
    }
    if (hasEvent(events, IOEvent::Write)) {
        wsaEvents |= FD_WRITE;
    }

    // Always monitor for close and connect events
    wsaEvents |= FD_CLOSE | FD_CONNECT;

    return wsaEvents;
}

#else

// Placeholder implementation for non-Windows platforms
namespace dht_hunter::network {

namespace {
    static auto logger = dht_hunter::logforge::LogForge::getLogger("WSAMultiplexer");
}

WSAMultiplexer::WSAMultiplexer() {
    logger->warning("WSAMultiplexer is not supported on this platform");
}

WSAMultiplexer::~WSAMultiplexer() {
}

bool WSAMultiplexer::addSocket(Socket* /* socket */, int /* events */, IOEventCallback /* callback */) {
    logger->warning("WSAMultiplexer is not supported on this platform");
    return false;
}

bool WSAMultiplexer::modifySocket(Socket* /* socket */, int /* events */) {
    logger->warning("WSAMultiplexer is not supported on this platform");
    return false;
}

bool WSAMultiplexer::removeSocket(Socket* /* socket */) {
    logger->warning("WSAMultiplexer is not supported on this platform");
    return false;
}

int WSAMultiplexer::wait(std::chrono::milliseconds /* timeout */) {
    logger->warning("WSAMultiplexer is not supported on this platform");
    return -1;
}

long WSAMultiplexer::toWSAEvents(int /* events */) const {
    return 0;
}

#endif

} // namespace dht_hunter::network
