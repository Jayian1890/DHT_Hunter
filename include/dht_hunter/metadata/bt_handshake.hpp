#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace dht_hunter::metadata {

/**
 * @brief BitTorrent protocol name
 */
constexpr const char* BT_PROTOCOL_NAME = "BitTorrent protocol";

/**
 * @brief Length of the protocol name
 */
constexpr uint8_t BT_PROTOCOL_NAME_LENGTH = 19;

/**
 * @brief Length of the reserved bytes in the handshake
 */
constexpr size_t BT_RESERVED_BYTES_LENGTH = 8;

/**
 * @brief Length of the info hash in the handshake
 */
constexpr size_t BT_INFOHASH_LENGTH = 20;

/**
 * @brief Length of the peer ID in the handshake
 */
constexpr size_t BT_PEER_ID_LENGTH = 20;

/**
 * @brief Total length of the handshake message
 */
constexpr size_t BT_HANDSHAKE_LENGTH = 1 + BT_PROTOCOL_NAME_LENGTH + BT_RESERVED_BYTES_LENGTH + BT_INFOHASH_LENGTH + BT_PEER_ID_LENGTH;

/**
 * @brief Bit flag for extension protocol support (BEP 10)
 * This is bit 20 from the right in the reserved bytes (byte 5, bit 4)
 */
constexpr uint8_t BT_EXTENSION_PROTOCOL_FLAG = 0x10;

/**
 * @brief Structure representing a BitTorrent handshake message
 */
struct BTHandshake {
    uint8_t protocolLength;                              ///< Length of the protocol name (always 19)
    std::array<char, BT_PROTOCOL_NAME_LENGTH> protocol;  ///< Protocol name ("BitTorrent protocol")
    std::array<uint8_t, BT_RESERVED_BYTES_LENGTH> reserved; ///< Reserved bytes for feature flags
    std::array<uint8_t, BT_INFOHASH_LENGTH> infoHash;    ///< Info hash of the torrent
    std::array<uint8_t, BT_PEER_ID_LENGTH> peerId;       ///< Peer ID

    /**
     * @brief Default constructor
     */
    BTHandshake();

    /**
     * @brief Constructor with info hash and peer ID
     * @param infoHash The info hash of the torrent
     * @param peerId The peer ID
     * @param enableExtensions Whether to enable the extension protocol
     */
    BTHandshake(const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash,
                const std::array<uint8_t, BT_PEER_ID_LENGTH>& peerId,
                bool enableExtensions = true);

    /**
     * @brief Checks if the extension protocol is supported
     * @return True if the extension protocol is supported, false otherwise
     */
    bool supportsExtensionProtocol() const;

    /**
     * @brief Enables or disables the extension protocol
     * @param enable Whether to enable the extension protocol
     */
    void setExtensionProtocol(bool enable);

    /**
     * @brief Serializes the handshake to a byte array
     * @return The serialized handshake
     */
    std::vector<uint8_t> serialize() const;

    /**
     * @brief Deserializes a handshake from a byte array
     * @param data The byte array
     * @param length The length of the byte array
     * @return True if deserialization was successful, false otherwise
     */
    bool deserialize(const uint8_t* data, size_t length);
};

/**
 * @brief Generates a random peer ID
 * @param prefix The prefix to use for the peer ID (e.g., "-DH0001-")
 * @return The generated peer ID
 */
std::array<uint8_t, BT_PEER_ID_LENGTH> generatePeerId(const std::string& prefix = "-DH0001-");

} // namespace dht_hunter::metadata
