#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <system_error>

namespace dht_hunter::network {

// Forward declarations
class EndPoint;

/**
 * @enum SocketType
 * @brief Defines the type of socket.
 */
enum class SocketType {
    TCP,
    UDP
};

/**
 * @enum SocketError
 * @brief Defines common socket error codes.
 */
enum class SocketError {
    None,
    InvalidSocket,
    AddressInUse,
    ConnectionRefused,
    NetworkUnreachable,
    TimedOut,
    WouldBlock,
    HostUnreachable,
    AddressNotAvailable,
    NotConnected,
    Interrupted,
    OperationInProgress,
    AlreadyConnected,
    NotBound,
    Unknown
};

/**
 * @class Socket
 * @brief Abstract base class for all socket types.
 * 
 * This class provides a common interface for both TCP and UDP sockets,
 * abstracting away platform-specific details.
 */
class Socket {
public:
    /**
     * @brief Virtual destructor for proper cleanup.
     */
    virtual ~Socket() = default;
    
    /**
     * @brief Binds the socket to a local endpoint.
     * @param localEndpoint The local endpoint to bind to.
     * @return True if successful, false otherwise.
     */
    virtual bool bind(const EndPoint& localEndpoint) = 0;
    
    /**
     * @brief Connects the socket to a remote endpoint.
     * @param remoteEndpoint The remote endpoint to connect to.
     * @return True if successful, false otherwise.
     */
    virtual bool connect(const EndPoint& remoteEndpoint) = 0;
    
    /**
     * @brief Closes the socket.
     */
    virtual void close() = 0;
    
    /**
     * @brief Sends data through the socket.
     * @param data Pointer to the data to send.
     * @param length Length of the data to send.
     * @return Number of bytes sent, or -1 on error.
     */
    virtual int send(const uint8_t* data, size_t length) = 0;
    
    /**
     * @brief Receives data from the socket.
     * @param buffer Buffer to store received data.
     * @param maxLength Maximum length of data to receive.
     * @return Number of bytes received, or -1 on error.
     */
    virtual int receive(uint8_t* buffer, size_t maxLength) = 0;
    
    /**
     * @brief Sets the socket to blocking or non-blocking mode.
     * @param nonBlocking True for non-blocking mode, false for blocking mode.
     * @return True if successful, false otherwise.
     */
    virtual bool setNonBlocking(bool nonBlocking) = 0;
    
    /**
     * @brief Sets the socket to reuse address.
     * @param reuse True to enable address reuse, false to disable.
     * @return True if successful, false otherwise.
     */
    virtual bool setReuseAddress(bool reuse) = 0;
    
    /**
     * @brief Gets the last socket error.
     * @return The last socket error.
     */
    virtual SocketError getLastError() const = 0;
    
    /**
     * @brief Gets the socket type.
     * @return The socket type (TCP or UDP).
     */
    virtual SocketType getType() const = 0;
    
    /**
     * @brief Checks if the socket is valid.
     * @return True if the socket is valid, false otherwise.
     */
    virtual bool isValid() const = 0;
    
    /**
     * @brief Gets a string representation of a socket error.
     * @param error The socket error.
     * @return A string describing the error.
     */
    static std::string getErrorString(SocketError error);
};

} // namespace dht_hunter::network
