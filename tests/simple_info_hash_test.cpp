#include <iostream>
#include <string>
#include <array>
#include <algorithm>
#include <functional>

// Define InfoHash as an alias for std::array<uint8_t, 20>
using InfoHash = std::array<uint8_t, 20>;

/**
 * @brief Create an empty info hash (all zeros)
 * @return An empty info hash
 */
InfoHash createEmptyInfoHash() {
    InfoHash hash;
    std::fill(hash.begin(), hash.end(), 0);
    return hash;
}

/**
 * @brief Check if an info hash is valid (not all zeros)
 * @param hash The info hash to check
 * @return True if the info hash is valid, false otherwise
 */
bool isValidInfoHash(const InfoHash& hash) {
    return !std::all_of(hash.begin(), hash.end(), [](uint8_t b) { return b == 0; });
}

/**
 * @brief Convert an info hash to a hex string
 * @param hash The info hash to convert
 * @return The hex string representation of the info hash
 */
std::string infoHashToString(const InfoHash& hash) {
    std::string result;
    result.reserve(40);
    
    for (size_t i = 0; i < hash.size(); ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", hash[i]);
        result += buf;
    }
    
    return result;
}

/**
 * @brief Convert a hex string to an info hash
 * @param str The hex string to convert
 * @param hash The info hash to store the result in
 * @return True if the conversion was successful, false otherwise
 */
bool infoHashFromString(const std::string& str, InfoHash& hash) {
    if (str.length() != 40) {
        return false;
    }
    
    for (size_t i = 0; i < 20; ++i) {
        std::string byteStr = str.substr(i * 2, 2);
        try {
            hash[i] = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
        } catch (const std::exception&) {
            return false;
        }
    }
    
    return true;
}

// Specialization of std::hash for InfoHash
namespace std {
    template<>
    struct hash<InfoHash> {
        size_t operator()(const InfoHash& hash) const {
            size_t result = 0;
            for (size_t i = 0; i < hash.size(); ++i) {
                result = result * 31 + hash[i];
            }
            return result;
        }
    };
}

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
 * @brief Main function
 * @return 0 if all tests pass, 1 otherwise
 */
int main() {
    // Run the tests
    bool allTestsPassed = true;
    
    allTestsPassed &= testCreateEmptyInfoHash();
    allTestsPassed &= testInfoHashToString();
    
    if (allTestsPassed) {
        std::cout << "All InfoHash tests passed!" << std::endl;
        return 0;
    } else {
        std::cerr << "Some InfoHash tests failed!" << std::endl;
        return 1;
    }
}
