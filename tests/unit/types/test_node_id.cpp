#include "dht_hunter/types/node_id.hpp"
#include <iostream>
#include <string>
#include <array>
#include <algorithm>
#include <cassert>

using namespace dht_hunter::types;

/**
 * @brief Test the default constructor
 * @return True if the test passes, false otherwise
 */
bool testDefaultConstructor() {
    std::cout << "Testing default constructor..." << std::endl;
    
    NodeID nodeID;
    
    // Check that all bytes are zero
    for (size_t i = 0; i < nodeID.size(); ++i) {
        if (nodeID[i] != 0) {
            std::cerr << "Default constructor did not initialize all bytes to zero" << std::endl;
            return false;
        }
    }
    
    // Check that the node ID is not valid (all zeros)
    if (nodeID.isValid()) {
        std::cerr << "Default constructor created a valid node ID (should be invalid)" << std::endl;
        return false;
    }
    
    std::cout << "Default constructor test passed!" << std::endl;
    return true;
}

/**
 * @brief Test the constructor from bytes
 * @return True if the test passes, false otherwise
 */
bool testBytesConstructor() {
    std::cout << "Testing bytes constructor..." << std::endl;
    
    // Create a byte array with known values
    std::array<uint8_t, 20> bytes;
    for (size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<uint8_t>(i);
    }
    
    // Create a node ID from the bytes
    NodeID nodeID(bytes);
    
    // Check that the bytes match
    for (size_t i = 0; i < nodeID.size(); ++i) {
        if (nodeID[i] != bytes[i]) {
            std::cerr << "Bytes constructor did not initialize bytes correctly" << std::endl;
            return false;
        }
    }
    
    // Check that the node ID is valid (not all zeros)
    if (!nodeID.isValid()) {
        std::cerr << "Bytes constructor created an invalid node ID" << std::endl;
        return false;
    }
    
    std::cout << "Bytes constructor test passed!" << std::endl;
    return true;
}

/**
 * @brief Test the constructor from a hex string
 * @return True if the test passes, false otherwise
 */
bool testHexStringConstructor() {
    std::cout << "Testing hex string constructor..." << std::endl;
    
    // Create a hex string with known values
    std::string hexString = "0102030405060708090a0b0c0d0e0f1011121314";
    
    // Create a node ID from the hex string
    NodeID nodeID(hexString);
    
    // Check that the bytes match
    for (size_t i = 0; i < nodeID.size(); ++i) {
        uint8_t expected = static_cast<uint8_t>(i + 1);
        if (nodeID[i] != expected) {
            std::cerr << "Hex string constructor did not initialize bytes correctly" << std::endl;
            std::cerr << "Expected: " << static_cast<int>(expected) << ", Got: " << static_cast<int>(nodeID[i]) << " at index " << i << std::endl;
            return false;
        }
    }
    
    // Check that the node ID is valid (not all zeros)
    if (!nodeID.isValid()) {
        std::cerr << "Hex string constructor created an invalid node ID" << std::endl;
        return false;
    }
    
    // Test with an invalid hex string (too short)
    NodeID invalidNodeID1("0102");
    if (invalidNodeID1.isValid()) {
        std::cerr << "Hex string constructor accepted an invalid hex string (too short)" << std::endl;
        return false;
    }
    
    // Test with an invalid hex string (non-hex characters)
    NodeID invalidNodeID2("01020304050607080g0a0b0c0d0e0f1011121314");
    if (invalidNodeID2.isValid()) {
        std::cerr << "Hex string constructor accepted an invalid hex string (non-hex characters)" << std::endl;
        return false;
    }
    
    std::cout << "Hex string constructor test passed!" << std::endl;
    return true;
}

/**
 * @brief Test the equality and inequality operators
 * @return True if the test passes, false otherwise
 */
bool testEqualityOperators() {
    std::cout << "Testing equality operators..." << std::endl;
    
    // Create two identical node IDs
    std::array<uint8_t, 20> bytes1;
    std::array<uint8_t, 20> bytes2;
    for (size_t i = 0; i < bytes1.size(); ++i) {
        bytes1[i] = static_cast<uint8_t>(i);
        bytes2[i] = static_cast<uint8_t>(i);
    }
    
    NodeID nodeID1(bytes1);
    NodeID nodeID2(bytes2);
    
    // Check equality
    if (!(nodeID1 == nodeID2)) {
        std::cerr << "Equality operator failed for identical node IDs" << std::endl;
        return false;
    }
    
    // Check inequality
    if (nodeID1 != nodeID2) {
        std::cerr << "Inequality operator failed for identical node IDs" << std::endl;
        return false;
    }
    
    // Modify one byte and check again
    bytes2[10] = 42;
    NodeID nodeID3(bytes2);
    
    // Check equality
    if (nodeID1 == nodeID3) {
        std::cerr << "Equality operator failed for different node IDs" << std::endl;
        return false;
    }
    
    // Check inequality
    if (!(nodeID1 != nodeID3)) {
        std::cerr << "Inequality operator failed for different node IDs" << std::endl;
        return false;
    }
    
    std::cout << "Equality operators test passed!" << std::endl;
    return true;
}

/**
 * @brief Test the less than operator
 * @return True if the test passes, false otherwise
 */
bool testLessThanOperator() {
    std::cout << "Testing less than operator..." << std::endl;
    
    // Create two node IDs where the first is less than the second
    std::array<uint8_t, 20> bytes1;
    std::array<uint8_t, 20> bytes2;
    for (size_t i = 0; i < bytes1.size(); ++i) {
        bytes1[i] = static_cast<uint8_t>(i);
        bytes2[i] = static_cast<uint8_t>(i);
    }
    bytes1[0] = 1;
    bytes2[0] = 2;
    
    NodeID nodeID1(bytes1);
    NodeID nodeID2(bytes2);
    
    // Check less than
    if (!(nodeID1 < nodeID2)) {
        std::cerr << "Less than operator failed for node IDs" << std::endl;
        return false;
    }
    
    // Check not less than
    if (nodeID2 < nodeID1) {
        std::cerr << "Less than operator failed for node IDs (reverse check)" << std::endl;
        return false;
    }
    
    std::cout << "Less than operator test passed!" << std::endl;
    return true;
}

/**
 * @brief Test the distance calculation
 * @return True if the test passes, false otherwise
 */
bool testDistanceCalculation() {
    std::cout << "Testing distance calculation..." << std::endl;
    
    // Create two node IDs
    std::array<uint8_t, 20> bytes1;
    std::array<uint8_t, 20> bytes2;
    for (size_t i = 0; i < bytes1.size(); ++i) {
        bytes1[i] = static_cast<uint8_t>(i);
        bytes2[i] = static_cast<uint8_t>(i + 1);
    }
    
    NodeID nodeID1(bytes1);
    NodeID nodeID2(bytes2);
    
    // Calculate the distance
    NodeID distance = nodeID1.distanceTo(nodeID2);
    
    // Check the distance (should be XOR of each byte)
    for (size_t i = 0; i < distance.size(); ++i) {
        uint8_t expected = bytes1[i] ^ bytes2[i];
        if (distance[i] != expected) {
            std::cerr << "Distance calculation failed" << std::endl;
            std::cerr << "Expected: " << static_cast<int>(expected) << ", Got: " << static_cast<int>(distance[i]) << " at index " << i << std::endl;
            return false;
        }
    }
    
    std::cout << "Distance calculation test passed!" << std::endl;
    return true;
}

/**
 * @brief Test the random node ID generation
 * @return True if the test passes, false otherwise
 */
bool testRandomNodeID() {
    std::cout << "Testing random node ID generation..." << std::endl;
    
    // Generate a random node ID
    NodeID nodeID = NodeID::random();
    
    // Check that the node ID is valid (not all zeros)
    if (!nodeID.isValid()) {
        std::cerr << "Random node ID is invalid" << std::endl;
        return false;
    }
    
    // Generate another random node ID and check that they're different
    NodeID nodeID2 = NodeID::random();
    if (nodeID == nodeID2) {
        std::cerr << "Two random node IDs are identical (very unlikely)" << std::endl;
        return false;
    }
    
    std::cout << "Random node ID generation test passed!" << std::endl;
    return true;
}

/**
 * @brief Test the toString method
 * @return True if the test passes, false otherwise
 */
bool testToString() {
    std::cout << "Testing toString method..." << std::endl;
    
    // Create a node ID with known values
    std::array<uint8_t, 20> bytes;
    for (size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<uint8_t>(i);
    }
    
    NodeID nodeID(bytes);
    
    // Get the string representation
    std::string str = nodeID.toString();
    
    // Check the string length
    if (str.length() != 40) {
        std::cerr << "toString returned a string of incorrect length" << std::endl;
        std::cerr << "Expected: 40, Got: " << str.length() << std::endl;
        return false;
    }
    
    // Check the string content
    std::string expected = "000102030405060708090a0b0c0d0e0f10111213";
    if (str != expected) {
        std::cerr << "toString returned an incorrect string" << std::endl;
        std::cerr << "Expected: " << expected << ", Got: " << str << std::endl;
        return false;
    }
    
    std::cout << "toString method test passed!" << std::endl;
    return true;
}

/**
 * @brief Main function
 * @return 0 if all tests pass, 1 otherwise
 */
int main() {
    // Run the tests
    bool allTestsPassed = true;
    
    allTestsPassed &= testDefaultConstructor();
    allTestsPassed &= testBytesConstructor();
    allTestsPassed &= testHexStringConstructor();
    allTestsPassed &= testEqualityOperators();
    allTestsPassed &= testLessThanOperator();
    allTestsPassed &= testDistanceCalculation();
    allTestsPassed &= testRandomNodeID();
    allTestsPassed &= testToString();
    
    if (allTestsPassed) {
        std::cout << "All NodeID tests passed!" << std::endl;
        return 0;
    } else {
        std::cerr << "Some NodeID tests failed!" << std::endl;
        return 1;
    }
}
