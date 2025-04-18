#pragma once

#include "socket.hpp"
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <chrono>

namespace dht_hunter::network {

/**
 * @enum IOEvent
 * @brief Defines the types of I/O events that can be monitored.
 */
enum class IOEvent {
    Read,       ///< Socket is ready for reading
    Write,      ///< Socket is ready for writing
    Error,      ///< An error occurred on the socket
    Timeout     ///< A timeout occurred while waiting for events
};

/**
 * @brief Callback function type for I/O events.
 * @param socket The socket that triggered the event.
 * @param event The event that occurred.
 */
using IOEventCallback = std::function<void(Socket* socket, IOEvent event)>;

/**
 * @class IOMultiplexer
 * @brief Interface for I/O multiplexing.
 *
 * This class provides an interface for monitoring multiple sockets for I/O events.
 */
class IOMultiplexer {
public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~IOMultiplexer() = default;

    /**
     * @brief Adds a socket to be monitored for the specified events.
     * @param socket The socket to monitor.
     * @param events A bitmask of IOEvent values to monitor.
     * @param callback The callback to invoke when an event occurs.
     * @return True if the socket was added successfully, false otherwise.
     */
    virtual bool addSocket(Socket* socket, int events, IOEventCallback callback) = 0;

    /**
     * @brief Modifies the events to monitor for a socket.
     * @param socket The socket to modify.
     * @param events A bitmask of IOEvent values to monitor.
     * @return True if the socket was modified successfully, false otherwise.
     */
    virtual bool modifySocket(Socket* socket, int events) = 0;

    /**
     * @brief Removes a socket from the multiplexer.
     * @param socket The socket to remove.
     * @return True if the socket was removed successfully, false otherwise.
     */
    virtual bool removeSocket(Socket* socket) = 0;

    /**
     * @brief Waits for events on the monitored sockets.
     * @param timeout The maximum time to wait for events. Use a negative value to wait indefinitely.
     * @return The number of sockets that have events, or -1 on error.
     */
    virtual int wait(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)) = 0;

    /**
     * @brief Creates a platform-specific IOMultiplexer instance.
     * @return A unique pointer to the created IOMultiplexer.
     */
    static std::unique_ptr<IOMultiplexer> create();
};

/**
 * @brief Helper function to convert IOEvent to int.
 * @param event The IOEvent to convert.
 * @return The int representation of the event.
 */
inline int toInt(IOEvent event) {
    return 1 << static_cast<int>(event);
}

/**
 * @brief Helper function to check if an event is set in a bitmask.
 * @param events The bitmask of events.
 * @param event The event to check.
 * @return True if the event is set, false otherwise.
 */
inline bool hasEvent(int events, IOEvent event) {
    return (events & toInt(event)) != 0;
}

} // namespace dht_hunter::network
