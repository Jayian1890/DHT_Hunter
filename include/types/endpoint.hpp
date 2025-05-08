#pragma once

#include "../common.hpp"

namespace dht_hunter::types {

/**
 * @brief Represents a network endpoint (IP address and port)
 */
class EndPoint {
public:
    /**
     * @brief Default constructor
     */
    EndPoint();

    /**
     * @brief Constructor
     * @param ip The IP address
     * @param port The port
     */
    EndPoint(const std::string& ip, uint16_t port);

    /**
     * @brief Constructor from a string in the format "ip:port"
     * @param hostPort The string in the format "ip:port"
     */
    explicit EndPoint(const std::string& hostPort);

    /**
     * @brief Gets the IP address
     * @return The IP address
     */
    const std::string& getIp() const;

    /**
     * @brief Gets the port
     * @return The port
     */
    uint16_t getPort() const;

    /**
     * @brief Sets the IP address
     * @param ip The IP address
     */
    void setIp(const std::string& ip);

    /**
     * @brief Sets the port
     * @param port The port
     */
    void setPort(uint16_t port);

    /**
     * @brief Gets the string representation of the endpoint
     * @return The string representation
     */
    std::string toString() const;

    /**
     * @brief Checks if the endpoint is valid
     * @return True if the endpoint is valid, false otherwise
     */
    bool isValid() const;

    /**
     * @brief Equality operator
     * @param other The other endpoint
     * @return True if the endpoints are equal, false otherwise
     */
    bool operator==(const EndPoint& other) const;

    /**
     * @brief Inequality operator
     * @param other The other endpoint
     * @return True if the endpoints are not equal, false otherwise
     */
    bool operator!=(const EndPoint& other) const;

    /**
     * @brief Less than operator (for use in maps/sets)
     * @param other The other endpoint
     * @return True if this endpoint is less than the other, false otherwise
     */
    bool operator<(const EndPoint& other) const;

private:
    std::string m_ip;
    uint16_t m_port;
};

} // namespace dht_hunter::types

// Hash function for EndPoint
namespace std {
    template<>
    struct hash<dht_hunter::types::EndPoint> {
        size_t operator()(const dht_hunter::types::EndPoint& endpoint) const {
            return std::hash<std::string>()(endpoint.getIp()) ^ 
                   (std::hash<uint16_t>()(endpoint.getPort()) << 1);
        }
    };
}
