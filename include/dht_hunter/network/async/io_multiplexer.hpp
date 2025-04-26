#pragma once

#include "dht_hunter/network/socket/socket.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace dht_hunter::network {

/**
 * @enum IOEvent
 * @brief Defines the I/O events that can be monitored.
 */
enum class IOEvent {
    Read = 1,
    Write = 2,
    Error = 4
};

/**
 * @brief Combines IOEvent values using bitwise OR.
 * @param lhs The left-hand side operand.
 * @param rhs The right-hand side operand.
 * @return The combined IOEvent.
 */
inline IOEvent operator|(IOEvent lhs, IOEvent rhs) {
    return static_cast<IOEvent>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

/**
 * @brief Checks if an IOEvent contains a specific event.
 * @param events The events to check.
 * @param event The event to check for.
 * @return True if the events contain the specified event, false otherwise.
 */
inline bool hasEvent(IOEvent events, IOEvent event) {
    return (static_cast<int>(events) & static_cast<int>(event)) != 0;
}

/**
 * @class IOMultiplexer
 * @brief Base class for I/O multiplexers.
 *
 * This class provides an interface for I/O multiplexers, which allow
 * monitoring multiple sockets for I/O events.
 */
class IOMultiplexer {
public:
    /**
     * @brief Callback for I/O events.
     * @param socket The socket that triggered the event.
     * @param events The events that occurred.
     */
    using IOEventCallback = std::function<void(std::shared_ptr<Socket> socket, IOEvent events)>;

    /**
     * @brief Virtual destructor.
     */
    virtual ~IOMultiplexer() = default;

    /**
     * @brief Adds a socket to the multiplexer.
     * @param socket The socket to add.
     * @param events The events to monitor.
     * @param callback The callback to call when an event occurs.
     * @return True if the socket was added, false otherwise.
     */
    virtual bool addSocket(std::shared_ptr<Socket> socket, IOEvent events, IOEventCallback callback) = 0;

    /**
     * @brief Modifies the events for a socket.
     * @param socket The socket to modify.
     * @param events The new events to monitor.
     * @return True if the socket was modified, false otherwise.
     */
    virtual bool modifySocket(std::shared_ptr<Socket> socket, IOEvent events) = 0;

    /**
     * @brief Removes a socket from the multiplexer.
     * @param socket The socket to remove.
     * @return True if the socket was removed, false otherwise.
     */
    virtual bool removeSocket(std::shared_ptr<Socket> socket) = 0;

    /**
     * @brief Waits for I/O events.
     * @param timeout The maximum time to wait.
     * @return The number of sockets that have events.
     */
    virtual int wait(std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)) = 0;

    /**
     * @brief Wakes up the multiplexer from a wait operation.
     */
    virtual void wakeup() = 0;

    /**
     * @brief Gets the number of sockets in the multiplexer.
     * @return The number of sockets.
     */
    virtual size_t getSocketCount() const = 0;

    /**
     * @brief Creates an I/O multiplexer.
     * @return A shared pointer to the created I/O multiplexer.
     */
    static std::shared_ptr<IOMultiplexer> create();
};

} // namespace dht_hunter::network
