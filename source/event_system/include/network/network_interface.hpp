#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <future>

namespace network {

/**
 * @brief Network address class
 */
class NetworkAddress {
public:
    /**
     * @brief Constructor
     * @param host The host name or IP address
     * @param port The port number
     */
    NetworkAddress(std::string host, uint16_t port)
        : m_host(std::move(host)), m_port(port) {}

    /**
     * @brief Gets the host name or IP address
     * @return The host name or IP address
     */
    const std::string& getHost() const { return m_host; }

    /**
     * @brief Gets the port number
     * @return The port number
     */
    uint16_t getPort() const { return m_port; }

    /**
     * @brief Gets a string representation of the address
     * @return A string representation of the address
     */
    std::string toString() const {
        return m_host + ":" + std::to_string(m_port);
    }

private:
    std::string m_host;
    uint16_t m_port;
};

/**
 * @brief Network message class
 */
class NetworkMessage {
public:
    /**
     * @brief Constructor
     * @param data The message data
     */
    explicit NetworkMessage(std::vector<uint8_t> data)
        : m_data(std::move(data)) {}

    /**
     * @brief Gets the message data
     * @return The message data
     */
    const std::vector<uint8_t>& getData() const { return m_data; }

    /**
     * @brief Gets the message size
     * @return The message size
     */
    size_t getSize() const { return m_data.size(); }

private:
    std::vector<uint8_t> m_data;
};

/**
 * @brief Network interface class
 */
class NetworkInterface {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~NetworkInterface() = default;

    /**
     * @brief Opens the network interface
     * @return A future that will be fulfilled when the operation completes
     */
    virtual std::future<bool> open() = 0;

    /**
     * @brief Closes the network interface
     * @return A future that will be fulfilled when the operation completes
     */
    virtual std::future<void> close() = 0;

    /**
     * @brief Sends a message
     * @param address The destination address
     * @param message The message to send
     * @return A future that will be fulfilled when the operation completes
     */
    virtual std::future<bool> send(const NetworkAddress& address, const NetworkMessage& message) = 0;

    /**
     * @brief Checks if the network interface is open
     * @return True if the network interface is open, false otherwise
     */
    virtual bool isOpen() const = 0;
};

} // namespace network
