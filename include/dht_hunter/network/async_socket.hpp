#pragma once

#include "socket.hpp"
#include "io_multiplexer.hpp"
#include <functional>
#include <memory>
#include <vector>

namespace dht_hunter::network {

/**
 * @brief Callback for asynchronous read operations.
 * @param socket The socket that completed the read.
 * @param data The data that was read.
 * @param size The number of bytes read.
 * @param error The error code, or 0 if no error occurred.
 */
using AsyncReadCallback = std::function<void(Socket* socket, const uint8_t* data, int size, SocketError error)>;

/**
 * @brief Callback for asynchronous write operations.
 * @param socket The socket that completed the write.
 * @param size The number of bytes written.
 * @param error The error code, or 0 if no error occurred.
 */
using AsyncWriteCallback = std::function<void(Socket* socket, int size, SocketError error)>;

/**
 * @brief Callback for asynchronous accept operations.
 * @param socket The socket that accepted the connection.
 * @param newSocket The new socket for the accepted connection.
 * @param error The error code, or 0 if no error occurred.
 */
using AsyncAcceptCallback = std::function<void(Socket* socket, std::unique_ptr<Socket> newSocket, SocketError error)>;

/**
 * @brief Callback for asynchronous connect operations.
 * @param socket The socket that completed the connection.
 * @param error The error code, or 0 if no error occurred.
 */
using AsyncConnectCallback = std::function<void(Socket* socket, SocketError error)>;

/**
 * @class AsyncSocket
 * @brief Wrapper for Socket that provides asynchronous operations.
 */
class AsyncSocket {
public:
    /**
     * @brief Constructor.
     * @param socket The socket to wrap.
     * @param multiplexer The I/O multiplexer to use.
     */
    AsyncSocket(std::unique_ptr<Socket> socket, std::shared_ptr<IOMultiplexer> multiplexer);

    /**
     * @brief Destructor.
     */
    ~AsyncSocket();

    /**
     * @brief Gets the underlying socket.
     * @return The underlying socket.
     */
    Socket* getSocket() const;

    /**
     * @brief Asynchronously reads data from the socket.
     * @param buffer The buffer to read into.
     * @param size The maximum number of bytes to read.
     * @param callback The callback to invoke when the read completes.
     * @return True if the read was initiated successfully, false otherwise.
     */
    bool asyncRead(uint8_t* buffer, int size, AsyncReadCallback callback);

    /**
     * @brief Asynchronously writes data to the socket.
     * @param data The data to write.
     * @param size The number of bytes to write.
     * @param callback The callback to invoke when the write completes.
     * @return True if the write was initiated successfully, false otherwise.
     */
    bool asyncWrite(const uint8_t* data, int size, AsyncWriteCallback callback);

    /**
     * @brief Asynchronously accepts a connection.
     * @param callback The callback to invoke when the accept completes.
     * @return True if the accept was initiated successfully, false otherwise.
     */
    bool asyncAccept(AsyncAcceptCallback callback);

    /**
     * @brief Asynchronously connects to a remote endpoint.
     * @param endpoint The endpoint to connect to.
     * @param callback The callback to invoke when the connect completes.
     * @return True if the connect was initiated successfully, false otherwise.
     */
    bool asyncConnect(const EndPoint& endpoint, AsyncConnectCallback callback);

    /**
     * @brief Cancels all pending asynchronous operations.
     */
    void cancelAll();

private:
    /**
     * @brief Handles I/O events from the multiplexer.
     * @param socket The socket that triggered the event.
     * @param event The event that occurred.
     */
    void handleEvent(Socket* socket, IOEvent event);

    /**
     * @brief Structure for a pending read operation.
     */
    struct PendingRead {
        uint8_t* buffer;           ///< Buffer to read into
        int size;                  ///< Maximum number of bytes to read
        AsyncReadCallback callback; ///< Callback to invoke when the read completes
    };

    /**
     * @brief Structure for a pending write operation.
     */
    struct PendingWrite {
        std::vector<uint8_t> data; ///< Data to write
        int offset;               ///< Offset into the data
        AsyncWriteCallback callback; ///< Callback to invoke when the write completes
    };

    std::unique_ptr<Socket> m_socket;                ///< The underlying socket
    std::shared_ptr<IOMultiplexer> m_multiplexer;    ///< The I/O multiplexer
    PendingRead m_pendingRead;                      ///< The pending read operation
    PendingWrite m_pendingWrite;                    ///< The pending write operation
    AsyncAcceptCallback m_acceptCallback;           ///< The pending accept callback
    AsyncConnectCallback m_connectCallback;         ///< The pending connect callback
    bool m_readPending;                             ///< Whether a read operation is pending
    bool m_writePending;                            ///< Whether a write operation is pending
    bool m_acceptPending;                           ///< Whether an accept operation is pending
    bool m_connectPending;                          ///< Whether a connect operation is pending
};

} // namespace dht_hunter::network
