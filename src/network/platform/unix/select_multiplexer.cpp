#include "dht_hunter/network/platform/unix/select_multiplexer.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include <algorithm>
#include <cstring>
#include <sys/time.h>
#include <unistd.h>

namespace dht_hunter::network {

SelectMultiplexer::SelectMultiplexer() : m_maxFd(-1), m_wakeupFlag(false) {
    FD_ZERO(&m_readSet);
    FD_ZERO(&m_writeSet);
    FD_ZERO(&m_errorSet);
    
    // Create a pipe for wakeup
    if (pipe(m_wakeupPipe) == -1) {
        // Failed to create pipe
        m_wakeupPipe[0] = -1;
        m_wakeupPipe[1] = -1;
    } else {
        // Set the read end of the pipe to non-blocking mode
        int flags = fcntl(m_wakeupPipe[0], F_GETFL, 0);
        fcntl(m_wakeupPipe[0], F_SETFL, flags | O_NONBLOCK);
        
        // Update the maximum file descriptor
        m_maxFd = std::max(m_maxFd, m_wakeupPipe[0]);
    }
}

SelectMultiplexer::~SelectMultiplexer() {
    // Close the wakeup pipe
    if (m_wakeupPipe[0] != -1) {
        close(m_wakeupPipe[0]);
    }
    if (m_wakeupPipe[1] != -1) {
        close(m_wakeupPipe[1]);
    }
}

bool SelectMultiplexer::addSocket(std::shared_ptr<Socket> socket, IOEvent events, IOEventCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!socket || !socket->isValid()) {
        return false;
    }
    
    int fd = socket->getSocketDescriptor();
    
    // Check if the socket is already in the multiplexer
    if (m_sockets.find(fd) != m_sockets.end()) {
        return false;
    }
    
    // Add the socket to the map
    SocketInfo info;
    info.socket = socket;
    info.events = events;
    info.callback = callback;
    m_sockets[fd] = info;
    
    // Update the maximum file descriptor
    m_maxFd = std::max(m_maxFd, fd);
    
    return true;
}

bool SelectMultiplexer::modifySocket(std::shared_ptr<Socket> socket, IOEvent events) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!socket || !socket->isValid()) {
        return false;
    }
    
    int fd = socket->getSocketDescriptor();
    
    // Check if the socket is in the multiplexer
    auto it = m_sockets.find(fd);
    if (it == m_sockets.end()) {
        return false;
    }
    
    // Update the events
    it->second.events = events;
    
    return true;
}

bool SelectMultiplexer::removeSocket(std::shared_ptr<Socket> socket) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!socket) {
        return false;
    }
    
    int fd = socket->getSocketDescriptor();
    
    // Check if the socket is in the multiplexer
    auto it = m_sockets.find(fd);
    if (it == m_sockets.end()) {
        return false;
    }
    
    // Remove the socket from the map
    m_sockets.erase(it);
    
    // Recalculate the maximum file descriptor if this was the maximum
    if (fd == m_maxFd) {
        m_maxFd = m_wakeupPipe[0];
        for (const auto& pair : m_sockets) {
            m_maxFd = std::max(m_maxFd, pair.first);
        }
    }
    
    return true;
}

int SelectMultiplexer::wait(std::chrono::milliseconds timeout) {
    // Prepare the fd sets
    fd_set readSet;
    fd_set writeSet;
    fd_set errorSet;
    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);
    FD_ZERO(&errorSet);
    
    int maxFd = -1;
    std::unordered_map<int, SocketInfo> sockets;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Add the wakeup pipe to the read set
        if (m_wakeupPipe[0] != -1) {
            FD_SET(m_wakeupPipe[0], &readSet);
            maxFd = std::max(maxFd, m_wakeupPipe[0]);
        }
        
        // Add the sockets to the fd sets
        for (const auto& pair : m_sockets) {
            int fd = pair.first;
            const SocketInfo& info = pair.second;
            
            if (hasEvent(info.events, IOEvent::Read)) {
                FD_SET(fd, &readSet);
            }
            
            if (hasEvent(info.events, IOEvent::Write)) {
                FD_SET(fd, &writeSet);
            }
            
            FD_SET(fd, &errorSet);
            
            maxFd = std::max(maxFd, fd);
        }
        
        // Copy the sockets map to avoid holding the lock during select()
        sockets = m_sockets;
        
        // Reset the wakeup flag
        m_wakeupFlag.store(false);
    }
    
    // Prepare the timeout
    struct timeval tv;
    struct timeval* tvp = nullptr;
    
    if (timeout.count() >= 0) {
        tv.tv_sec = static_cast<time_t>(timeout.count() / 1000);
        tv.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);
        tvp = &tv;
    }
    
    // Wait for events
    int result = ::select(maxFd + 1, &readSet, &writeSet, &errorSet, tvp);
    
    if (result < 0) {
        // Error
        if (errno == EINTR) {
            // Interrupted by signal
            return 0;
        }
        
        // Other error
        return -1;
    }
    
    if (result == 0) {
        // Timeout
        return 0;
    }
    
    // Check if the wakeup pipe was triggered
    if (m_wakeupPipe[0] != -1 && FD_ISSET(m_wakeupPipe[0], &readSet)) {
        // Read from the pipe to clear it
        char buffer[256];
        while (read(m_wakeupPipe[0], buffer, sizeof(buffer)) > 0) {
            // Discard the data
        }
    }
    
    // Process the events
    for (const auto& pair : sockets) {
        int fd = pair.first;
        const SocketInfo& info = pair.second;
        
        IOEvent events = static_cast<IOEvent>(0);
        
        if (FD_ISSET(fd, &readSet) && hasEvent(info.events, IOEvent::Read)) {
            events = events | IOEvent::Read;
        }
        
        if (FD_ISSET(fd, &writeSet) && hasEvent(info.events, IOEvent::Write)) {
            events = events | IOEvent::Write;
        }
        
        if (FD_ISSET(fd, &errorSet)) {
            events = events | IOEvent::Error;
        }
        
        if (static_cast<int>(events) != 0 && info.callback) {
            info.callback(info.socket, events);
        }
    }
    
    return result;
}

void SelectMultiplexer::wakeup() {
    if (m_wakeupPipe[1] != -1 && !m_wakeupFlag.exchange(true)) {
        // Write to the pipe to wake up select()
        char byte = 0;
        write(m_wakeupPipe[1], &byte, 1);
    }
}

size_t SelectMultiplexer::getSocketCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sockets.size();
}

} // namespace dht_hunter::network
