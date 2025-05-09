#include "dht_hunter/types/info_hash.hpp"
#include <iostream>
#include <string>
#include <array>
#include <algorithm>
#include <cassert>

using namespace dht_hunter::types;

/**
 * @brief Test creating an empty info hash
 * @return True if the test passes, false otherwise
 */
bool testCreateEmptyInfoHash() {
    std::cout << "Testing createEmptyInfoHash..." << std::endl;
    
    InfoHash emptyHash = createEmptyInfoHash();
    
    // Check that all bytes are zero
    for (size_t i = 0; i < emptyHash.size(); ++i) {
        if (emptyHash[i] != 0) {
            std::cerr << "createEmptyInfoHash did not initialize all bytes to zero" << std::endl;
            return false;
        }
    }
    
    // Check that the info hash is not valid (all zeros)
    if (isValidInfoHash(emptyHash)) {
        std::cerr << "Empty info hash is considered valid (should be invalid)" << std::endl;
        return false;
    }
    
    std::cout << "createEmptyInfoHash test passed!" << std::endl;
    return true;
}

/**
 * @brief Test converting an info hash to a string
 * @return True if the test passes, false otherwise
 */
bool testInfoHashToString() {
    std::cout << "Testing infoHashToString..." << std::endl;
    
    // Create an info hash with known values
    InfoHash infoHash;
    for (size_t i = 0; i < infoHash.size(); ++i) {
        infoHash[i] = static_cast<uint8_t>(i);
    }
    
    // Convert to string
    std::string str = infoHashToString(infoHash);
    
    // Check the string length
    if (str.length() != 40) {
        std::cerr << "infoHashToString returned a string of incorrect length" << std::endl;
        std::cerr << "Expected: 40, Got: " << str.length() << std::endl;
        return false;
    }
    
    // Check the string content
    std::string expected = "000102030405060708090a0b0c0d0e0f10111213";
    if (str != expected) {
        std::cerr << "infoHashToString returned an incorrect string" << std::endl;
        std::cerr << "Expected: " << expected << ", Got: " << str << std::endl;
        return false;
    }
    
    std::cout << "infoHashToString test passed!" << std::endl;
    return true;
}

/**
 * @brief Test converting a string to an info hash
 * @return True if the test passes, false otherwise
 */
bool testInfoHashFromString() {
    std::cout << "Testing infoHashFromString..." << std::endl;
    
    // Create a string with known values
    std::string str = "000102030405060708090a0b0c0d0e0f10111213";
    
    // Convert to info hash
    InfoHash infoHash;
    bool result = infoHashFromString(str, infoHash);
    
    // Check the result
    if (!result) {
        std::cerr << "infoHashFromString failed for a valid string" << std::endl;
        return false;
    }
    
    // Check the info hash content
    for (size_t i = 0; i < infoHash.size(); ++i) {
        uint8_t expected = static_cast<uint8_t>(i);
        if (infoHash[i] != expected) {
            std::cerr << "infoHashFromString created an incorrect info hash" << std::endl;
            std::cerr << "Expected: " << static_cast<int>(expected) << ", Got: " << static_cast<int>(infoHash[i]) << " at index " << i << std::endl;
            return false;
        }
    }
    
    // Test with an invalid string (too short)
    std::string invalidStr1 = "0102";
    InfoHash invalidHash1;
    result = infoHashFromString(invalidStr1, invalidHash1);
    if (result) {
        std::cerr << "infoHashFromString succeeded for an invalid string (too short)" << std::endl;
        return false;
    }
    
    // Test with an invalid string (non-hex characters)
    std::string invalidStr2 = "01020304050607080g0a0b0c0d0e0f1011121314";
    InfoHash invalidHash2;
    result = infoHashFromString(invalidStr2, invalidHash2);
    if (result) {
        std::cerr << "infoHashFromString succeeded for an invalid string (non-hex characters)" << std::endl;
        return false;
    }
    
    std::cout << "infoHashFromString test passed!" << std::endl;
    return true;
}

/**
 * @brief Test checking if an info hash is valid
 * @return True if the test passes, false otherwise
 */
bool testIsValidInfoHash() {
    std::cout << "Testing isValidInfoHash..." << std::endl;
    
    // Create an empty info hash (all zeros)
    InfoHash emptyHash = createEmptyInfoHash();
    
    // Check that it's not valid
    if (isValidInfoHash(emptyHash)) {
        std::cerr << "isValidInfoHash returned true for an empty info hash" << std::endl;
        return false;
    }
    
    // Create a valid info hash
    InfoHash validHash;
    for (size_t i = 0; i < validHash.size(); ++i) {
        validHash[i] = static_cast<uint8_t>(i + 1);
    }
    
    // Check that it's valid
    if (!isValidInfoHash(validHash)) {
        std::cerr << "isValidInfoHash returned false for a valid info hash" << std::endl;
        return false;
    }
    
    std::cout << "isValidInfoHash test passed!" << std::endl;
    return true;
}

/**
 * @brief Test the std::hash implementation for InfoHash
 * @return True if the test passes, false otherwise
 */
bool testInfoHashHash() {
    std::cout << "Testing std::hash<InfoHash>..." << std::endl;
    
    // Create two different info hashes
    InfoHash hash1;
    InfoHash hash2;
    
    for (size_t i = 0; i < hash1.size(); ++i) {
        hash1[i] = static_cast<uint8_t>(i);
        hash2[i] = static_cast<uint8_t>(i + 1);
    }
    
    // Calculate hashes
    std::hash<InfoHash> hasher;
    size_t hashValue1 = hasher(hash1);
    size_t hashValue2 = hasher(hash2);
    
    // Check that the hashes are different
    if (hashValue1 == hashValue2) {
        std::cerr << "std::hash<InfoHash> returned the same hash for different info hashes" << std::endl;
        return false;
    }
    
    // Create a copy of hash1
    InfoHash hash1Copy = hash1;
    
    // Calculate hash for the copy
    size_t hashValue1Copy = hasher(hash1Copy);
    
    // Check that the hashes are the same
    if (hashValue1 != hashValue1Copy) {
        std::cerr << "std::hash<InfoHash> returned different hashes for identical info hashes" << std::endl;
        return false;
    }
    
    std::cout << "std::hash<InfoHash> test passed!" << std::endl;
    return true;
}

/**
 * @brief Main function
 * @return 0 if all tests pass, 1 otherwise
 */
int main() {
    // Run the tests
    bool allTestsPassed = true;
    
    allTestsPassed &= testCreateEmptyInfoHash();
    allTestsPassed &= testInfoHashToString();
    allTestsPassed &= testInfoHashFromString();
    allTestsPassed &= testIsValidInfoHash();
    allTestsPassed &= testInfoHashHash();
    
    if (allTestsPassed) {
        std::cout << "All InfoHash tests passed!" << std::endl;
        return 0;
    } else {
        std::cerr << "Some InfoHash tests failed!" << std::endl;
        return 1;
    }
}
