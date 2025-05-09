#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <arpa/inet.h>

/**
 * @brief Class representing a network address (IPv4 or IPv6)
 */
class NetworkAddress {
private:
    std::vector<uint8_t> address;
    bool valid;
    bool ipv4;

public:
    /**
     * @brief Default constructor (creates 0.0.0.0)
     */
    NetworkAddress() : address(4, 0), valid(true), ipv4(true) {}

    /**
     * @brief Constructor from a string
     * @param addressStr The address string (e.g., "192.168.1.1" or "2001:0db8:85a3:0000:0000:8a2e:0370:7334")
     */
    NetworkAddress(const std::string& addressStr) : valid(false), ipv4(false) {
        // Try to parse as IPv4
        struct in_addr ipv4Addr;
        if (inet_pton(AF_INET, addressStr.c_str(), &ipv4Addr) == 1) {
            address.resize(4);
            memcpy(address.data(), &ipv4Addr.s_addr, 4);
            valid = true;
            ipv4 = true;
            return;
        }

        // Try to parse as IPv6
        struct in6_addr ipv6Addr;
        if (inet_pton(AF_INET6, addressStr.c_str(), &ipv6Addr) == 1) {
            address.resize(16);
            memcpy(address.data(), &ipv6Addr.s6_addr, 16);
            valid = true;
            ipv4 = false;
            return;
        }
    }

    /**
     * @brief Constructor from bytes
     * @param bytes The address bytes
     */
    NetworkAddress(const std::vector<uint8_t>& bytes) : address(bytes), valid(false), ipv4(false) {
        if (bytes.size() == 4) {
            valid = true;
            ipv4 = true;
        } else if (bytes.size() == 16) {
            valid = true;
            ipv4 = false;
        }
    }

    /**
     * @brief Check if the address is valid
     * @return True if the address is valid, false otherwise
     */
    bool isValid() const {
        return valid;
    }

    /**
     * @brief Check if the address is IPv4
     * @return True if the address is IPv4, false otherwise
     */
    bool isIPv4() const {
        return ipv4;
    }

    /**
     * @brief Check if the address is IPv6
     * @return True if the address is IPv6, false otherwise
     */
    bool isIPv6() const {
        return valid && !ipv4;
    }

    /**
     * @brief Convert the address to a string
     * @return The string representation of the address
     */
    std::string toString() const {
        if (!valid) {
            return "invalid";
        }

        char buf[INET6_ADDRSTRLEN];
        if (ipv4) {
            struct in_addr ipv4Addr;
            memcpy(&ipv4Addr.s_addr, address.data(), 4);
            inet_ntop(AF_INET, &ipv4Addr, buf, INET_ADDRSTRLEN);
        } else {
            struct in6_addr ipv6Addr;
            memcpy(&ipv6Addr.s6_addr, address.data(), 16);
            inet_ntop(AF_INET6, &ipv6Addr, buf, INET6_ADDRSTRLEN);
        }

        return std::string(buf);
    }

    /**
     * @brief Get the address bytes
     * @return The address bytes
     */
    std::vector<uint8_t> toBytes() const {
        return address;
    }

    /**
     * @brief Equality operator
     * @param other The other address to compare with
     * @return True if the addresses are equal, false otherwise
     */
    bool operator==(const NetworkAddress& other) const {
        return valid == other.valid && ipv4 == other.ipv4 && address == other.address;
    }

    /**
     * @brief Inequality operator
     * @param other The other address to compare with
     * @return True if the addresses are not equal, false otherwise
     */
    bool operator!=(const NetworkAddress& other) const {
        return !(*this == other);
    }
};

/**
 * @brief Class representing an endpoint (address + port)
 */
class EndPoint {
private:
    NetworkAddress address;
    uint16_t port;

public:
    /**
     * @brief Constructor
     * @param addr The network address
     * @param p The port
     */
    EndPoint(const NetworkAddress& addr, uint16_t p) : address(addr), port(p) {}

    /**
     * @brief Get the network address
     * @return The network address
     */
    const NetworkAddress& getAddress() const {
        return address;
    }

    /**
     * @brief Get the port
     * @return The port
     */
    uint16_t getPort() const {
        return port;
    }

    /**
     * @brief Convert the endpoint to a string
     * @return The string representation of the endpoint
     */
    std::string toString() const {
        if (address.isIPv6()) {
            return "[" + address.toString() + "]:" + std::to_string(port);
        } else {
            return address.toString() + ":" + std::to_string(port);
        }
    }

    /**
     * @brief Equality operator
     * @param other The other endpoint to compare with
     * @return True if the endpoints are equal, false otherwise
     */
    bool operator==(const EndPoint& other) const {
        return address == other.address && port == other.port;
    }

    /**
     * @brief Inequality operator
     * @param other The other endpoint to compare with
     * @return True if the endpoints are not equal, false otherwise
     */
    bool operator!=(const EndPoint& other) const {
        return !(*this == other);
    }
};

/**
 * @brief Test the NetworkAddress class
 * @return True if all tests pass, false otherwise
 */
bool testNetworkAddress() {
    std::cout << "Testing NetworkAddress..." << std::endl;
    
    // Test default constructor
    NetworkAddress defaultAddr;
    if (!defaultAddr.isValid()) {
        std::cerr << "Default NetworkAddress is not valid" << std::endl;
        return false;
    }
    if (!defaultAddr.isIPv4()) {
        std::cerr << "Default NetworkAddress is not IPv4" << std::endl;
        return false;
    }
    if (defaultAddr.toString() != "0.0.0.0") {
        std::cerr << "Default NetworkAddress is not 0.0.0.0" << std::endl;
        return false;
    }
    
    // Test constructor with IPv4 string
    NetworkAddress ipv4Addr("192.168.1.1");
    if (!ipv4Addr.isValid()) {
        std::cerr << "IPv4 NetworkAddress is not valid" << std::endl;
        return false;
    }
    if (!ipv4Addr.isIPv4()) {
        std::cerr << "IPv4 NetworkAddress is not IPv4" << std::endl;
        return false;
    }
    if (ipv4Addr.isIPv6()) {
        std::cerr << "IPv4 NetworkAddress is IPv6" << std::endl;
        return false;
    }
    if (ipv4Addr.toString() != "192.168.1.1") {
        std::cerr << "IPv4 NetworkAddress toString() failed" << std::endl;
        return false;
    }
    
    std::cout << "NetworkAddress tests passed!" << std::endl;
    return true;
}

/**
 * @brief Test the EndPoint class
 * @return True if all tests pass, false otherwise
 */
bool testEndPoint() {
    std::cout << "Testing EndPoint..." << std::endl;
    
    // Test constructor
    NetworkAddress addr("192.168.1.1");
    uint16_t port = 6881;
    EndPoint endpoint(addr, port);
    
    // Test getters
    if (!(endpoint.getAddress() == addr)) {
        std::cerr << "EndPoint getAddress() failed" << std::endl;
        return false;
    }
    
    if (endpoint.getPort() != port) {
        std::cerr << "EndPoint getPort() failed" << std::endl;
        return false;
    }
    
    // Test toString method
    if (endpoint.toString() != "192.168.1.1:6881") {
        std::cerr << "EndPoint toString() failed for IPv4" << std::endl;
        std::cerr << "Expected: 192.168.1.1:6881, Got: " << endpoint.toString() << std::endl;
        return false;
    }
    
    std::cout << "EndPoint tests passed!" << std::endl;
    return true;
}

/**
 * @brief Main function
 * @return 0 if all tests pass, 1 otherwise
 */
int main() {
    // Run the tests
    bool allTestsPassed = true;
    
    allTestsPassed &= testNetworkAddress();
    allTestsPassed &= testEndPoint();
    
    if (allTestsPassed) {
        std::cout << "All EndPoint tests passed!" << std::endl;
        return 0;
    } else {
        std::cerr << "Some EndPoint tests failed!" << std::endl;
        return 1;
    }
}
