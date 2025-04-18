#pragma once

#include "../../io_multiplexer.hpp"
#include "../../platform/socket_impl.hpp"
#include <sys/select.h>
#include <unordered_map>
#include <vector>

namespace dht_hunter::network {

/**
 * @class SelectMultiplexer
 * @brief Unix implementation of IOMultiplexer using select().
 */
class SelectMultiplexer : public IOMultiplexer {
public:
    /**
     * @brief Constructor.
     */
    SelectMultiplexer();

    /**
     * @brief Destructor.
     */
    ~SelectMultiplexer() override;

    /**
     * @brief Adds a socket to be monitored for the specified events.
     * @param socket The socket to monitor.
     * @param events A bitmask of IOEvent values to monitor.
     * @param callback The callback to invoke when an event occurs.
     * @return True if the socket was added successfully, false otherwise.
     */
    bool addSocket(Socket* socket, int events, IOEventCallback callback) override;

    /**
     * @brief Modifies the events to monitor for a socket.
     * @param socket The socket to modify.
     * @param events A bitmask of IOEvent values to monitor.
     * @return True if the socket was modified successfully, false otherwise.
     */
    bool modifySocket(Socket* socket, int events) override;

    /**
     * @brief Removes a socket from the multiplexer.
     * @param socket The socket to remove.
     * @return True if the socket was removed successfully, false otherwise.
     */
    bool removeSocket(Socket* socket) override;

    /**
     * @brief Waits for events on the monitored sockets.
     * @param timeout The maximum time to wait for events. Use a negative value to wait indefinitely.
     * @return The number of sockets that have events, or -1 on error.
     */
    int wait(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)) override;

private:
    /**
     * @brief Structure to store socket information.
     */
    struct SocketInfo {
        int events;                 ///< Events to monitor
        IOEventCallback callback;   ///< Callback to invoke when an event occurs
    };

    std::unordered_map<SocketHandle, SocketInfo> m_sockets;  ///< Map of socket handles to socket info
    std::unordered_map<SocketHandle, Socket*> m_socketPtrs;  ///< Map of socket handles to socket pointers
    fd_set m_readSet;                                       ///< Set of sockets to monitor for read events
    fd_set m_writeSet;                                      ///< Set of sockets to monitor for write events
    fd_set m_errorSet;                                      ///< Set of sockets to monitor for error events
    SocketHandle m_maxHandle;                               ///< Maximum socket handle value
};

} // namespace dht_hunter::network
