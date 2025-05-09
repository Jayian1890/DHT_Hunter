#pragma once

/**
 * @file network_utils.hpp
 * @brief Consolidated network utilities
 *
 * This file contains consolidated network components including:
 * - UDP socket, server, and client
 * - HTTP server and client
 * - Network address and endpoint
 * - NAT traversal services
 */

// Standard library includes
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <map>
#include <unordered_map>
#include <sys/types.h>  // for ssize_t

// Project includes
#include "utils/common_utils.hpp"
#include "utils/system_utils.hpp"
#include "dht_hunter/types/endpoint.hpp"

namespace dht_hunter::utils::network {

//=============================================================================
// Constants
//=============================================================================

/**
 * @brief Default MTU size for UDP packets
 */
constexpr size_t DEFAULT_MTU_SIZE = 1400;

/**
 * @brief Default UDP port
 */
constexpr uint16_t DEFAULT_UDP_PORT = 6881;

/**
 * @brief Default HTTP port
 */
constexpr uint16_t DEFAULT_HTTP_PORT = 8080;

/**
 * @brief Default socket timeout in milliseconds
 */
constexpr int DEFAULT_SOCKET_TIMEOUT_MS = 1000;

//=============================================================================
// UDP Socket
//=============================================================================

/**
 * @class UDPSocket
 * @brief A simple UDP socket implementation for sending and receiving datagrams.
 */
class UDPSocket {
public:
    /**
     * @brief Default constructor.
     */
    UDPSocket();

    /**
     * @brief Destructor.
     */
    ~UDPSocket();

    /**
     * @brief Bind the socket to a specific port.
     * @param port The port to bind to.
     * @return True if successful, false otherwise.
     */
    bool bind(uint16_t port);

    /**
     * @brief Send data to a specific address and port.
     * @param data The data to send.
     * @param size The size of the data.
     * @param address The destination address.
     * @param port The destination port.
     * @return The number of bytes sent, or -1 on error.
     */
    ssize_t sendTo(const void* data, size_t size, const std::string& address, uint16_t port);

    /**
     * @brief Receive data from any address and port.
     * @param buffer The buffer to receive data into.
     * @param size The size of the buffer.
     * @param address The source address (output).
     * @param port The source port (output).
     * @return The number of bytes received, or -1 on error.
     */
    ssize_t receiveFrom(void* buffer, size_t size, std::string& address, uint16_t& port);

    /**
     * @brief Start the receive loop.
     * @return True if successful, false otherwise.
     */
    bool startReceiveLoop();

    /**
     * @brief Stop the receive loop.
     */
    void stopReceiveLoop();

    /**
     * @brief Set a callback function to be called when data is received.
     * @param callback The callback function.
     */
    void setReceiveCallback(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback);

    /**
     * @brief Check if the socket is valid.
     * @return True if the socket is valid, false otherwise.
     */
    bool isValid() const;

    /**
     * @brief Close the socket.
     */
    void close();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

//=============================================================================
// UDP Server
//=============================================================================

/**
 * @class UDPServer
 * @brief A simple UDP server that listens for incoming datagrams.
 */
class UDPServer {
public:
    /**
     * @brief Default constructor.
     */
    UDPServer();

    /**
     * @brief Destructor.
     */
    ~UDPServer();

    /**
     * @brief Start the server on the specified port.
     * @param port The port to listen on.
     * @return True if successful, false otherwise.
     */
    bool start(uint16_t port);

    /**
     * @brief Start the server using the port from the configuration.
     * @return True if successful, false otherwise.
     */
    bool start();

    /**
     * @brief Stop the server.
     */
    void stop();

    /**
     * @brief Check if the server is running.
     * @return True if the server is running, false otherwise.
     */
    bool isRunning() const;

    /**
     * @brief Send data to a specific client.
     * @param data The data to send.
     * @param address The client's address.
     * @param port The client's port.
     * @return The number of bytes sent, or -1 on error.
     */
    ssize_t sendTo(const std::vector<uint8_t>& data, const std::string& address, uint16_t port);

    /**
     * @brief Set a callback function to be called when data is received.
     * @param callback The callback function.
     */
    void setMessageHandler(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback);

private:
    UDPSocket m_socket;
    bool m_running;
};

//=============================================================================
// UDP Client
//=============================================================================

/**
 * @class UDPClient
 * @brief A simple UDP client for sending and receiving datagrams.
 */
class UDPClient {
public:
    /**
     * @brief Default constructor.
     */
    UDPClient();

    /**
     * @brief Destructor.
     */
    ~UDPClient();

    /**
     * @brief Start the client.
     * @return True if successful, false otherwise.
     */
    bool start();

    /**
     * @brief Stop the client.
     */
    void stop();

    /**
     * @brief Check if the client is running.
     * @return True if the client is running, false otherwise.
     */
    bool isRunning() const;

    /**
     * @brief Send data to a specific server.
     * @param data The data to send.
     * @param address The server's address.
     * @param port The server's port.
     * @return The number of bytes sent, or -1 on error.
     */
    ssize_t send(const std::vector<uint8_t>& data, const std::string& address, uint16_t port);

    /**
     * @brief Set a callback function to be called when data is received.
     * @param callback The callback function.
     */
    void setMessageHandler(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback);

private:
    UDPSocket m_socket;
    bool m_running;
};

//=============================================================================
// HTTP Server (Forward declarations)
//=============================================================================

/**
 * @class HTTPServer
 * @brief A simple HTTP server.
 */
class HTTPServer {
public:
    /**
     * @brief Default constructor.
     */
    HTTPServer();

    /**
     * @brief Destructor.
     */
    ~HTTPServer();

    /**
     * @brief Start the server on the specified port.
     * @param port The port to listen on.
     * @return True if successful, false otherwise.
     */
    bool start(uint16_t port);

    /**
     * @brief Stop the server.
     */
    void stop();

    /**
     * @brief Check if the server is running.
     * @return True if the server is running, false otherwise.
     */
    bool isRunning() const;

    /**
     * @brief Set the web root directory.
     * @param webRoot The web root directory.
     */
    void setWebRoot(const std::string& webRoot);

    /**
     * @brief Set a callback function to be called when a request is received.
     * @param callback The callback function.
     */
    void setRequestHandler(std::function<void(const std::string&, const std::string&, std::string&)> callback);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

//=============================================================================
// HTTP Client (Forward declarations)
//=============================================================================

/**
 * @class HTTPClient
 * @brief A simple HTTP client.
 */
class HTTPClient {
public:
    /**
     * @brief Default constructor.
     */
    HTTPClient();

    /**
     * @brief Destructor.
     */
    ~HTTPClient();

    /**
     * @brief Send a GET request.
     * @param url The URL to send the request to.
     * @param response The response (output).
     * @return True if successful, false otherwise.
     */
    bool get(const std::string& url, std::string& response);

    /**
     * @brief Send a POST request.
     * @param url The URL to send the request to.
     * @param data The data to send.
     * @param response The response (output).
     * @return True if successful, false otherwise.
     */
    bool post(const std::string& url, const std::string& data, std::string& response);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace dht_hunter::utils::network
