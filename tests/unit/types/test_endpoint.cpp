#include "dht_hunter/types/endpoint.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <cassert>

using namespace dht_hunter::types;

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
    
    // Test constructor with IPv6 string
    NetworkAddress ipv6Addr("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
    if (!ipv6Addr.isValid()) {
        std::cerr << "IPv6 NetworkAddress is not valid" << std::endl;
        return false;
    }
    if (ipv6Addr.isIPv4()) {
        std::cerr << "IPv6 NetworkAddress is IPv4" << std::endl;
        return false;
    }
    if (!ipv6Addr.isIPv6()) {
        std::cerr << "IPv6 NetworkAddress is not IPv6" << std::endl;
        return false;
    }
    
    // Test constructor with invalid string
    NetworkAddress invalidAddr("invalid");
    if (invalidAddr.isValid()) {
        std::cerr << "Invalid NetworkAddress is valid" << std::endl;
        return false;
    }
    
    // Test constructor with IPv4 bytes
    std::vector<uint8_t> ipv4Bytes = {192, 168, 1, 1};
    NetworkAddress ipv4BytesAddr(ipv4Bytes);
    if (!ipv4BytesAddr.isValid()) {
        std::cerr << "IPv4 bytes NetworkAddress is not valid" << std::endl;
        return false;
    }
    if (!ipv4BytesAddr.isIPv4()) {
        std::cerr << "IPv4 bytes NetworkAddress is not IPv4" << std::endl;
        return false;
    }
    
    // Test constructor with IPv6 bytes
    std::vector<uint8_t> ipv6Bytes = {
        0x20, 0x01, 0x0d, 0xb8, 0x85, 0xa3, 0x00, 0x00,
        0x00, 0x00, 0x8a, 0x2e, 0x03, 0x70, 0x73, 0x34
    };
    NetworkAddress ipv6BytesAddr(ipv6Bytes);
    if (!ipv6BytesAddr.isValid()) {
        std::cerr << "IPv6 bytes NetworkAddress is not valid" << std::endl;
        return false;
    }
    if (!ipv6BytesAddr.isIPv6()) {
        std::cerr << "IPv6 bytes NetworkAddress is not IPv6" << std::endl;
        return false;
    }
    
    // Test constructor with invalid bytes
    std::vector<uint8_t> invalidBytes = {192, 168, 1};
    NetworkAddress invalidBytesAddr(invalidBytes);
    if (invalidBytesAddr.isValid()) {
        std::cerr << "Invalid bytes NetworkAddress is valid" << std::endl;
        return false;
    }
    
    // Test toBytes method
    std::vector<uint8_t> ipv4ToBytes = ipv4Addr.toBytes();
    if (ipv4ToBytes.size() != 4) {
        std::cerr << "IPv4 toBytes() returned wrong size" << std::endl;
        return false;
    }
    
    std::vector<uint8_t> ipv6ToBytes = ipv6Addr.toBytes();
    if (ipv6ToBytes.size() != 16) {
        std::cerr << "IPv6 toBytes() returned wrong size" << std::endl;
        return false;
    }
    
    // Test equality operators
    NetworkAddress addr1("192.168.1.1");
    NetworkAddress addr2("192.168.1.1");
    NetworkAddress addr3("192.168.1.2");
    
    if (!(addr1 == addr2)) {
        std::cerr << "Equality operator failed for identical addresses" << std::endl;
        return false;
    }
    
    if (addr1 != addr2) {
        std::cerr << "Inequality operator failed for identical addresses" << std::endl;
        return false;
    }
    
    if (addr1 == addr3) {
        std::cerr << "Equality operator failed for different addresses" << std::endl;
        return false;
    }
    
    if (!(addr1 != addr3)) {
        std::cerr << "Inequality operator failed for different addresses" << std::endl;
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
    
    // Test IPv6 endpoint toString
    NetworkAddress ipv6Addr("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
    EndPoint ipv6Endpoint(ipv6Addr, port);
    
    if (ipv6Endpoint.toString() != "[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:6881") {
        std::cerr << "EndPoint toString() failed for IPv6" << std::endl;
        std::cerr << "Expected: [2001:0db8:85a3:0000:0000:8a2e:0370:7334]:6881, Got: " << ipv6Endpoint.toString() << std::endl;
        return false;
    }
    
    // Test equality operators
    NetworkAddress addr1("192.168.1.1");
    NetworkAddress addr2("192.168.1.1");
    NetworkAddress addr3("192.168.1.2");
    
    EndPoint ep1(addr1, 6881);
    EndPoint ep2(addr2, 6881);
    EndPoint ep3(addr3, 6881);
    EndPoint ep4(addr1, 6882);
    
    if (!(ep1 == ep2)) {
        std::cerr << "Equality operator failed for identical endpoints" << std::endl;
        return false;
    }
    
    if (ep1 != ep2) {
        std::cerr << "Inequality operator failed for identical endpoints" << std::endl;
        return false;
    }
    
    if (ep1 == ep3) {
        std::cerr << "Equality operator failed for endpoints with different addresses" << std::endl;
        return false;
    }
    
    if (!(ep1 != ep3)) {
        std::cerr << "Inequality operator failed for endpoints with different addresses" << std::endl;
        return false;
    }
    
    if (ep1 == ep4) {
        std::cerr << "Equality operator failed for endpoints with different ports" << std::endl;
        return false;
    }
    
    if (!(ep1 != ep4)) {
        std::cerr << "Inequality operator failed for endpoints with different ports" << std::endl;
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
