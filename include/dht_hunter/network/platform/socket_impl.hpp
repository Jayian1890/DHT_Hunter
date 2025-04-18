#pragma once

#include "../socket.hpp"
#include "../network_address.hpp"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using SocketHandle = SOCKET;
    #define INVALID_SOCKET_HANDLE INVALID_SOCKET
    // Define ssize_t for Windows
    #if !defined(ssize_t)
        #if defined(_WIN64)
            typedef __int64 ssize_t;
        #else
            typedef int ssize_t;
        #endif
    #endif
    // Define TCP_NODELAY if not defined
    #ifndef TCP_NODELAY
        #define TCP_NODELAY 0x0001
    #endif
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <signal.h>
    #include <netinet/tcp.h>
    using SocketHandle = int;
    #define INVALID_SOCKET_HANDLE (-1)
#endif

namespace dht_hunter::network {

/**
 * @class SocketImpl
 * @brief Platform-specific socket implementation.
 *
 * This class provides a concrete implementation of the Socket interface
 * using platform-specific socket APIs.
 */
class SocketImpl : public Socket {
public:
    /**
     * @brief Constructs a socket with the specified type and address family.
     * @param type The socket type (TCP or UDP).
     * @param ipv6 Whether to create an IPv6 socket.
     */
    SocketImpl(SocketType type, bool ipv6);

    /**
     * @brief Constructs a socket from an existing socket handle.
     * @param handle The existing socket handle.
     * @param type The socket type (TCP or UDP).
     */
    SocketImpl(SocketHandle handle, SocketType type);

    /**
     * @brief Destructor that closes the socket if it's still open.
     */
    ~SocketImpl() override;

    // Socket interface implementation
    bool bind(const EndPoint& localEndpoint) override;
    bool connect(const EndPoint& remoteEndpoint) override;
    void close() override;
    int send(const uint8_t* data, size_t length) override;
    int receive(uint8_t* buffer, size_t maxLength) override;
    bool setNonBlocking(bool nonBlocking) override;
    bool setReuseAddress(bool reuse) override;
    SocketError getLastError() const override;
    SocketType getType() const override;
    bool isValid() const override;

    /**
     * @brief Gets the native socket handle.
     * @return The native socket handle.
     */
    SocketHandle getHandle() const;

protected:
    /**
     * @brief Converts a platform-specific error code to a SocketError.
     * @param errorCode The platform-specific error code.
     * @return The corresponding SocketError.
     */
    static SocketError translateError(int errorCode);

    /**
     * @brief Gets the last platform-specific error code.
     * @return The last platform-specific error code.
     */
    static int getLastErrorCode();

    SocketHandle m_handle;
    SocketType m_type;
    mutable SocketError m_lastError;
    bool m_ipv6;
};

/**
 * @class TCPSocketImpl
 * @brief TCP-specific socket implementation.
 *
 * This class extends SocketImpl with TCP-specific functionality.
 */
class TCPSocketImpl : public SocketImpl {
public:
    /**
     * @brief Constructs a TCP socket.
     * @param ipv6 Whether to create an IPv6 socket.
     */
    explicit TCPSocketImpl(bool ipv6 = false);

    /**
     * @brief Constructs a TCP socket from an existing socket handle.
     * @param handle The existing socket handle.
     */
    explicit TCPSocketImpl(SocketHandle handle);

    /**
     * @brief Listens for incoming connections.
     * @param backlog The maximum length of the queue of pending connections.
     * @return True if successful, false otherwise.
     */
    bool listen(int backlog = SOMAXCONN);

    /**
     * @brief Accepts an incoming connection.
     * @return A unique pointer to the accepted socket, or nullptr on error.
     */
    std::unique_ptr<TCPSocketImpl> accept();

    /**
     * @brief Sets the TCP no delay option (disables Nagle's algorithm).
     * @param noDelay True to disable Nagle's algorithm, false to enable it.
     * @return True if successful, false otherwise.
     */
    bool setNoDelay(bool noDelay);

    /**
     * @brief Sets the TCP keep alive option.
     * @param keepAlive True to enable keep alive, false to disable it.
     * @param idleTime The time (in seconds) the connection needs to remain idle before TCP starts sending keepalive probes.
     * @param interval The time (in seconds) between individual keepalive probes.
     * @param count The maximum number of keepalive probes TCP should send before dropping the connection.
     * @return True if successful, false otherwise.
     */
    bool setKeepAlive(bool keepAlive, int idleTime = 120, int interval = 30, int count = 8);
};

/**
 * @class UDPSocketImpl
 * @brief UDP-specific socket implementation.
 *
 * This class extends SocketImpl with UDP-specific functionality.
 */
class UDPSocketImpl : public SocketImpl {
public:
    /**
     * @brief Constructs a UDP socket.
     * @param ipv6 Whether to create an IPv6 socket.
     */
    explicit UDPSocketImpl(bool ipv6 = false);

    /**
     * @brief Constructs a UDP socket from an existing socket handle.
     * @param handle The existing socket handle.
     */
    explicit UDPSocketImpl(SocketHandle handle);

    /**
     * @brief Sends data to a specific endpoint.
     * @param data Pointer to the data to send.
     * @param length Length of the data to send.
     * @param destination The destination endpoint.
     * @return Number of bytes sent, or -1 on error.
     */
    int sendTo(const uint8_t* data, size_t length, const EndPoint& destination);

    /**
     * @brief Receives data from a socket and gets the source endpoint.
     * @param buffer Buffer to store received data.
     * @param maxLength Maximum length of data to receive.
     * @param source Will be filled with the source endpoint.
     * @return Number of bytes received, or -1 on error.
     */
    int receiveFrom(uint8_t* buffer, size_t maxLength, EndPoint& source);

    /**
     * @brief Sets the broadcast option.
     * @param broadcast True to enable broadcasting, false to disable it.
     * @return True if successful, false otherwise.
     */
    bool setBroadcast(bool broadcast);

    /**
     * @brief Joins a multicast group.
     * @param groupAddress The multicast group address.
     * @param interfaceAddress The interface address to use for the multicast group.
     * @return True if successful, false otherwise.
     */
    bool joinMulticastGroup(const NetworkAddress& groupAddress,
                           const NetworkAddress& interfaceAddress = NetworkAddress::any());

    /**
     * @brief Leaves a multicast group.
     * @param groupAddress The multicast group address.
     * @param interfaceAddress The interface address used for the multicast group.
     * @return True if successful, false otherwise.
     */
    bool leaveMulticastGroup(const NetworkAddress& groupAddress,
                            const NetworkAddress& interfaceAddress = NetworkAddress::any());
};

} // namespace dht_hunter::network
