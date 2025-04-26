#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <functional>
#include <algorithm>

namespace dht_hunter::dht {

/**
 * @brief A 160-bit (20-byte) node ID used in the DHT
 */
class NodeID {
public:
    /**
     * @brief Default constructor, initializes to all zeros
     */
    NodeID();

    /**
     * @brief Constructor from a byte array
     * @param bytes The byte array
     */
    explicit NodeID(const std::array<uint8_t, 20>& bytes);

    /**
     * @brief Constructor from a string
     * @param hexString The hex string representation
     */
    explicit NodeID(const std::string& hexString);

    /**
     * @brief Copy constructor
     * @param other The other NodeID
     */
    NodeID(const NodeID& other) = default;

    /**
     * @brief Move constructor
     * @param other The other NodeID
     */
    NodeID(NodeID&& other) noexcept = default;

    /**
     * @brief Copy assignment operator
     * @param other The other NodeID
     * @return Reference to this NodeID
     */
    NodeID& operator=(const NodeID& other) = default;

    /**
     * @brief Move assignment operator
     * @param other The other NodeID
     * @return Reference to this NodeID
     */
    NodeID& operator=(NodeID&& other) noexcept = default;

    /**
     * @brief Destructor
     */
    ~NodeID() = default;

    /**
     * @brief Equality operator
     * @param other The other NodeID
     * @return True if the NodeIDs are equal, false otherwise
     */
    bool operator==(const NodeID& other) const;

    /**
     * @brief Inequality operator
     * @param other The other NodeID
     * @return True if the NodeIDs are not equal, false otherwise
     */
    bool operator!=(const NodeID& other) const;

    /**
     * @brief Less than operator for sorting
     * @param other The other NodeID
     * @return True if this NodeID is less than the other, false otherwise
     */
    bool operator<(const NodeID& other) const;

    /**
     * @brief Array subscript operator
     * @param index The index
     * @return Reference to the byte at the index
     */
    uint8_t& operator[](size_t index);

    /**
     * @brief Array subscript operator (const version)
     * @param index The index
     * @return The byte at the index
     */
    uint8_t operator[](size_t index) const;

    /**
     * @brief Gets the size of the NodeID in bytes
     * @return The size in bytes
     */
    size_t size() const;

    /**
     * @brief Gets the raw byte array
     * @return The raw byte array
     */
    const std::array<uint8_t, 20>& bytes() const;

    /**
     * @brief Gets a pointer to the raw data
     * @return Pointer to the raw data
     */
    const uint8_t* data() const;

    /**
     * @brief Gets a pointer to the raw data
     * @return Pointer to the raw data
     */
    uint8_t* data();

    /**
     * @brief Gets an iterator to the beginning of the NodeID
     * @return Iterator to the beginning
     */
    auto begin() { return m_bytes.begin(); }

    /**
     * @brief Gets an iterator to the end of the NodeID
     * @return Iterator to the end
     */
    auto end() { return m_bytes.end(); }

    /**
     * @brief Gets a const iterator to the beginning of the NodeID
     * @return Const iterator to the beginning
     */
    auto begin() const { return m_bytes.begin(); }

    /**
     * @brief Gets a const iterator to the end of the NodeID
     * @return Const iterator to the end
     */
    auto end() const { return m_bytes.end(); }

    /**
     * @brief Converts the NodeID to a string
     * @return The string representation
     */
    std::string toString() const;

    /**
     * @brief Calculates the distance between this NodeID and another
     * @param other The other NodeID
     * @return The distance
     */
    NodeID distanceTo(const NodeID& other) const;

    /**
     * @brief Checks if the NodeID is valid
     * @return True if the NodeID is valid, false otherwise
     */
    bool isValid() const;

    /**
     * @brief Creates a NodeID with all bytes set to zero
     * @return A zero NodeID
     */
    static NodeID zero();

    /**
     * @brief Generates a random NodeID
     * @return A random NodeID
     */
    static NodeID random();

private:
    std::array<uint8_t, 20> m_bytes{};
};

/**
 * @brief Hash function for NodeID
 */
struct NodeIDHash {
    size_t operator()(const NodeID& nodeID) const {
        size_t result = 0;
        for (size_t i = 0; i < nodeID.size(); ++i) {
            result = result * 31 + nodeID[i];
        }
        return result;
    }
};

} // namespace dht_hunter::dht

// Add hash support for NodeID
namespace std {
    template<>
    struct hash<dht_hunter::dht::NodeID> {
        size_t operator()(const dht_hunter::dht::NodeID& nodeID) const {
            return dht_hunter::dht::NodeIDHash()(nodeID);
        }
    };
}
