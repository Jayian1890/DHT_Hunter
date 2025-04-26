#pragma once

#include "dht_hunter/network/address/endpoint.hpp"
#include "dht_hunter/network/async/io_multiplexer.hpp"
#include "dht_hunter/network/buffer/buffer.hpp"
#include "dht_hunter/network/socket/socket.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <queue>

namespace dht_hunter::network {

/**
 * @class AsyncSocket
 * @brief Base class for asynchronous sockets.
 *
 * This class provides an interface for asynchronous socket operations,
 * with support for non-blocking I/O and callbacks.
 */
class AsyncSocket {
public:
    /**
     * @brief Callback for connect operations.
     * @param success True if the operation was successful, false otherwise.
     */
    using ConnectCallback = std::function<void(bool success)>;

    /**
     * @brief Callback for send operations.
     * @param success True if the operation was successful, false otherwise.
     * @param bytesSent The number of bytes sent.
     */
    using SendCallback = std::function<void(bool success, int bytesSent)>;

    /**
     * @brief Callback for receive operations.
     * @param success True if the operation was successful, false otherwise.
     * @param buffer The received data buffer.
     * @param bytesReceived The number of bytes received.
     */
    using ReceiveCallback = std::function<void(bool success, std::shared_ptr<Buffer> buffer, int bytesReceived)>;

    /**
     * @brief Callback for close operations.
     */
    using CloseCallback = std::function<void()>;

    /**
     * @brief Virtual destructor.
     */
    virtual ~AsyncSocket() = default;

    /**
     * @brief Connects to a remote endpoint.
     * @param endpoint The endpoint to connect to.
     * @param callback The callback to call when the operation completes.
     */
    virtual void connect(const EndPoint& endpoint, ConnectCallback callback) = 0;

    /**
     * @brief Sends data to the remote endpoint.
     * @param buffer The data buffer to send.
     * @param callback The callback to call when the operation completes.
     */
    virtual void send(std::shared_ptr<Buffer> buffer, SendCallback callback) = 0;

    /**
     * @brief Receives data from the remote endpoint.
     * @param maxSize The maximum number of bytes to receive.
     * @param callback The callback to call when the operation completes.
     */
    virtual void receive(size_t maxSize, ReceiveCallback callback) = 0;

    /**
     * @brief Closes the socket.
     * @param callback The callback to call when the operation completes.
     */
    virtual void close(CloseCallback callback = nullptr) = 0;

    /**
     * @brief Gets the local endpoint of the socket.
     * @return The local endpoint.
     */
    virtual EndPoint getLocalEndpoint() const = 0;

    /**
     * @brief Gets the remote endpoint of the socket.
     * @return The remote endpoint.
     */
    virtual EndPoint getRemoteEndpoint() const = 0;

    /**
     * @brief Checks if the socket is connected.
     * @return True if the socket is connected, false otherwise.
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Gets the underlying socket.
     * @return The underlying socket.
     */
    virtual std::shared_ptr<Socket> getSocket() const = 0;

    /**
     * @brief Creates an asynchronous socket.
     * @param socket The underlying socket.
     * @param multiplexer The I/O multiplexer to use.
     * @return A shared pointer to the created asynchronous socket.
     */
    static std::shared_ptr<AsyncSocket> create(std::shared_ptr<Socket> socket, std::shared_ptr<IOMultiplexer> multiplexer);
};

/**
 * @class AsyncTCPSocket
 * @brief Asynchronous TCP socket.
 *
 * This class provides asynchronous operations for TCP sockets.
 */
class AsyncTCPSocket : public AsyncSocket {
public:
    /**
     * @brief Constructs an asynchronous TCP socket.
     * @param socket The underlying socket.
     * @param multiplexer The I/O multiplexer to use.
     */
    AsyncTCPSocket(std::shared_ptr<Socket> socket, std::shared_ptr<IOMultiplexer> multiplexer);

    /**
     * @brief Destructor.
     */
    virtual ~AsyncTCPSocket();

    /**
     * @brief Connects to a remote endpoint.
     * @param endpoint The endpoint to connect to.
     * @param callback The callback to call when the operation completes.
     */
    void connect(const EndPoint& endpoint, ConnectCallback callback) override;

    /**
     * @brief Sends data to the remote endpoint.
     * @param buffer The data buffer to send.
     * @param callback The callback to call when the operation completes.
     */
    void send(std::shared_ptr<Buffer> buffer, SendCallback callback) override;

    /**
     * @brief Receives data from the remote endpoint.
     * @param maxSize The maximum number of bytes to receive.
     * @param callback The callback to call when the operation completes.
     */
    void receive(size_t maxSize, ReceiveCallback callback) override;

    /**
     * @brief Closes the socket.
     * @param callback The callback to call when the operation completes.
     */
    void close(CloseCallback callback = nullptr) override;

    /**
     * @brief Gets the local endpoint of the socket.
     * @return The local endpoint.
     */
    EndPoint getLocalEndpoint() const override;

    /**
     * @brief Gets the remote endpoint of the socket.
     * @return The remote endpoint.
     */
    EndPoint getRemoteEndpoint() const override;

    /**
     * @brief Checks if the socket is connected.
     * @return True if the socket is connected, false otherwise.
     */
    bool isConnected() const override;

    /**
     * @brief Gets the underlying socket.
     * @return The underlying socket.
     */
    std::shared_ptr<Socket> getSocket() const override;

private:
    /**
     * @brief Handles I/O events.
     * @param events The events that occurred.
     */
    void handleIOEvent(IOEvent events);

    /**
     * @brief Handles a connect operation.
     */
    void handleConnect();

    /**
     * @brief Handles a send operation.
     */
    void handleSend();

    /**
     * @brief Handles a receive operation.
     */
    void handleReceive();

    /**
     * @brief Updates the I/O events.
     */
    void updateIOEvents();

    std::shared_ptr<Socket> m_socket;
    std::shared_ptr<IOMultiplexer> m_multiplexer;
    EndPoint m_remoteEndpoint;
    bool m_connected;
    bool m_connecting;
    bool m_closing;

    ConnectCallback m_connectCallback;
    std::queue<std::pair<std::shared_ptr<Buffer>, SendCallback>> m_sendQueue;
    std::shared_ptr<Buffer> m_currentSendBuffer;
    size_t m_currentSendOffset;
    SendCallback m_currentSendCallback;
    bool m_sending;

    std::queue<std::pair<size_t, ReceiveCallback>> m_receiveQueue;
    std::shared_ptr<Buffer> m_currentReceiveBuffer;
    ReceiveCallback m_currentReceiveCallback;
    bool m_receiving;

    CloseCallback m_closeCallback;

    mutable std::mutex m_mutex;
};

/**
 * @class AsyncUDPSocket
 * @brief Asynchronous UDP socket.
 *
 * This class provides asynchronous operations for UDP sockets.
 */
class AsyncUDPSocket : public AsyncSocket {
public:
    /**
     * @brief Callback for sendTo operations.
     * @param success True if the operation was successful, false otherwise.
     * @param bytesSent The number of bytes sent.
     */
    using SendToCallback = std::function<void(bool success, int bytesSent)>;

    /**
     * @brief Callback for receiveFrom operations.
     * @param success True if the operation was successful, false otherwise.
     * @param buffer The received data buffer.
     * @param bytesReceived The number of bytes received.
     * @param endpoint The endpoint that sent the data.
     */
    using ReceiveFromCallback = std::function<void(bool success, std::shared_ptr<Buffer> buffer, int bytesReceived, const EndPoint& endpoint)>;

    /**
     * @brief Constructs an asynchronous UDP socket.
     * @param socket The underlying socket.
     * @param multiplexer The I/O multiplexer to use.
     */
    AsyncUDPSocket(std::shared_ptr<Socket> socket, std::shared_ptr<IOMultiplexer> multiplexer);

    /**
     * @brief Destructor.
     */
    virtual ~AsyncUDPSocket();

    /**
     * @brief Connects to a remote endpoint.
     * @param endpoint The endpoint to connect to.
     * @param callback The callback to call when the operation completes.
     */
    void connect(const EndPoint& endpoint, ConnectCallback callback) override;

    /**
     * @brief Sends data to the remote endpoint.
     * @param buffer The data buffer to send.
     * @param callback The callback to call when the operation completes.
     */
    void send(std::shared_ptr<Buffer> buffer, SendCallback callback) override;

    /**
     * @brief Receives data from the remote endpoint.
     * @param maxSize The maximum number of bytes to receive.
     * @param callback The callback to call when the operation completes.
     */
    void receive(size_t maxSize, ReceiveCallback callback) override;

    /**
     * @brief Closes the socket.
     * @param callback The callback to call when the operation completes.
     */
    void close(CloseCallback callback = nullptr) override;

    /**
     * @brief Gets the local endpoint of the socket.
     * @return The local endpoint.
     */
    EndPoint getLocalEndpoint() const override;

    /**
     * @brief Gets the remote endpoint of the socket.
     * @return The remote endpoint.
     */
    EndPoint getRemoteEndpoint() const override;

    /**
     * @brief Checks if the socket is connected.
     * @return True if the socket is connected, false otherwise.
     */
    bool isConnected() const override;

    /**
     * @brief Gets the underlying socket.
     * @return The underlying socket.
     */
    std::shared_ptr<Socket> getSocket() const override;

    /**
     * @brief Sends data to a specific endpoint.
     * @param buffer The data buffer to send.
     * @param endpoint The endpoint to send to.
     * @param callback The callback to call when the operation completes.
     */
    void sendTo(std::shared_ptr<Buffer> buffer, const EndPoint& endpoint, SendToCallback callback);

    /**
     * @brief Receives data from any endpoint.
     * @param maxSize The maximum number of bytes to receive.
     * @param callback The callback to call when the operation completes.
     */
    void receiveFrom(size_t maxSize, ReceiveFromCallback callback);

private:
    /**
     * @brief Handles I/O events.
     * @param events The events that occurred.
     */
    void handleIOEvent(IOEvent events);

    /**
     * @brief Handles a sendTo operation.
     */
    void handleSendTo();

    /**
     * @brief Handles a receiveFrom operation.
     */
    void handleReceiveFrom();

    /**
     * @brief Updates the I/O events.
     */
    void updateIOEvents();

    std::shared_ptr<Socket> m_socket;
    std::shared_ptr<IOMultiplexer> m_multiplexer;
    EndPoint m_remoteEndpoint;
    bool m_connected;
    bool m_closing;

    std::queue<std::tuple<std::shared_ptr<Buffer>, EndPoint, SendToCallback>> m_sendToQueue;
    std::shared_ptr<Buffer> m_currentSendBuffer;
    EndPoint m_currentSendEndpoint;
    SendToCallback m_currentSendToCallback;
    bool m_sending;

    std::queue<std::pair<size_t, ReceiveFromCallback>> m_receiveFromQueue;
    std::shared_ptr<Buffer> m_currentReceiveBuffer;
    ReceiveFromCallback m_currentReceiveFromCallback;
    bool m_receiving;

    CloseCallback m_closeCallback;

    mutable std::mutex m_mutex;
};

} // namespace dht_hunter::network
