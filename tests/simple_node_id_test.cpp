#include <iostream>
#include <string>
#include <array>
#include <random>
#include <algorithm>

// Simplified NodeID class for testing
class NodeID {
private:
    std::array<uint8_t, 20> id;
    bool valid;

public:
    // Default constructor
    NodeID() : id{}, valid(false) {
        std::fill(id.begin(), id.end(), 0);
    }

    // Constructor from bytes
    NodeID(const std::array<uint8_t, 20>& bytes) : id(bytes), valid(true) {
        // Check if all bytes are zero
        valid = !std::all_of(id.begin(), id.end(), [](uint8_t b) { return b == 0; });
    }

    // Constructor from hex string
    NodeID(const std::string& hexString) : id{}, valid(false) {
        if (hexString.length() != 40) {
            return;
        }

        for (size_t i = 0; i < 20; ++i) {
            std::string byteStr = hexString.substr(i * 2, 2);
            try {
                id[i] = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
            } catch (const std::exception&) {
                return;
            }
        }

        valid = !std::all_of(id.begin(), id.end(), [](uint8_t b) { return b == 0; });
    }

    // Check if the node ID is valid
    bool isValid() const {
        return valid;
    }

    // Get the size of the node ID
    size_t size() const {
        return id.size();
    }

    // Access operator
    uint8_t operator[](size_t index) const {
        return id[index];
    }

    // Equality operator
    bool operator==(const NodeID& other) const {
        return id == other.id;
    }

    // Inequality operator
    bool operator!=(const NodeID& other) const {
        return !(*this == other);
    }

    // Less than operator
    bool operator<(const NodeID& other) const {
        return id < other.id;
    }

    // Calculate the distance to another node ID
    NodeID distanceTo(const NodeID& other) const {
        std::array<uint8_t, 20> distance;
        for (size_t i = 0; i < id.size(); ++i) {
            distance[i] = id[i] ^ other.id[i];
        }
        return NodeID(distance);
    }

    // Generate a random node ID
    static NodeID random() {
        std::array<uint8_t, 20> bytes;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        for (size_t i = 0; i < bytes.size(); ++i) {
            bytes[i] = static_cast<uint8_t>(dis(gen));
        }

        return NodeID(bytes);
    }

    // Convert to string
    std::string toString() const {
        std::string result;
        result.reserve(40);
        
        for (size_t i = 0; i < id.size(); ++i) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02x", id[i]);
            result += buf;
        }
        
        return result;
    }
};

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
 * @brief Main function
 * @return 0 if all tests pass, 1 otherwise
 */
int main() {
    // Run the tests
    bool allTestsPassed = true;
    
    allTestsPassed &= testDefaultConstructor();
    allTestsPassed &= testBytesConstructor();
    
    if (allTestsPassed) {
        std::cout << "All NodeID tests passed!" << std::endl;
        return 0;
    } else {
        std::cerr << "Some NodeID tests failed!" << std::endl;
        return 1;
    }
}
