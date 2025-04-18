#pragma once

#include "../../io_multiplexer.hpp"
#include "../../platform/socket_impl.hpp"
#include <winsock2.h>
#include <windows.h>
#include <unordered_map>
#include <vector>

namespace dht_hunter::network {

/**
 * @class WSAMultiplexer
 * @brief Windows implementation of IOMultiplexer using WSAEventSelect.
 */
class WSAMultiplexer : public IOMultiplexer {
public:
    /**
     * @brief Constructor.
     */
    WSAMultiplexer();

    /**
     * @brief Destructor.
     */
    ~WSAMultiplexer() override;

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
     * @brief Converts IOEvent bitmask to WSA event bitmask.
     * @param events The IOEvent bitmask.
     * @return The WSA event bitmask.
     */
    long toWSAEvents(int events) const;

    /**
     * @brief Structure to store socket information.
     */
    struct SocketInfo {
        int events;                 ///< Events to monitor
        IOEventCallback callback;   ///< Callback to invoke when an event occurs
        WSAEVENT wsaEvent;          ///< WSA event handle
    };

    std::unordered_map<SocketHandle, SocketInfo> m_sockets;  ///< Map of socket handles to socket info
    std::vector<WSAEVENT> m_events;                         ///< Vector of WSA event handles
    std::vector<SocketHandle> m_handles;                     ///< Vector of socket handles
};

} // namespace dht_hunter::network
