#pragma once

#include "dht_hunter/bencode/bencode.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace dht_hunter::metadata {

/**
 * @brief BitTorrent extension message ID
 */
constexpr uint8_t BT_EXTENSION_MESSAGE_ID = 20;

/**
 * @brief Extension handshake message ID
 */
constexpr uint8_t BT_EXTENSION_HANDSHAKE_ID = 0;

/**
 * @class ExtensionProtocol
 * @brief Handles the BitTorrent extension protocol (BEP 10)
 */
class ExtensionProtocol {
public:
    /**
     * @brief Default constructor
     */
    ExtensionProtocol();

    /**
     * @brief Creates an extension handshake message
     * @param localPort The local TCP port
     * @param clientVersion The client version string
     * @param metadataSize The size of the metadata (if known, 0 otherwise)
     * @return The serialized handshake message
     */
    std::vector<uint8_t> createHandshake(uint16_t localPort, 
                                         const std::string& clientVersion,
                                         uint32_t metadataSize = 0) const;

    /**
     * @brief Processes a received extension handshake message
     * @param data The message data
     * @param length The message length
     * @return True if the handshake was processed successfully, false otherwise
     */
    bool processHandshake(const uint8_t* data, size_t length);

    /**
     * @brief Checks if the peer supports a specific extension
     * @param extensionName The name of the extension
     * @return True if the extension is supported, false otherwise
     */
    bool supportsExtension(const std::string& extensionName) const;

    /**
     * @brief Gets the message ID for a specific extension
     * @param extensionName The name of the extension
     * @return The message ID, or 0 if the extension is not supported
     */
    uint8_t getExtensionId(const std::string& extensionName) const;

    /**
     * @brief Gets the metadata size from the handshake
     * @return The metadata size, or 0 if not available
     */
    uint32_t getMetadataSize() const;

    /**
     * @brief Gets the peer's client version
     * @return The client version string, or empty if not available
     */
    std::string getClientVersion() const;

    /**
     * @brief Gets the peer's TCP port
     * @return The TCP port, or 0 if not available
     */
    uint16_t getPeerPort() const;

    /**
     * @brief Creates an extension message
     * @param extensionName The name of the extension
     * @param payload The payload data
     * @return The serialized extension message
     */
    std::vector<uint8_t> createExtensionMessage(const std::string& extensionName, 
                                               const std::vector<uint8_t>& payload) const;

    /**
     * @brief Processes a received extension message
     * @param data The message data
     * @param length The message length
     * @param extensionId The extension ID
     * @param extensionName Output parameter for the extension name
     * @param payload Output parameter for the payload data
     * @return True if the message was processed successfully, false otherwise
     */
    bool processExtensionMessage(const uint8_t* data, size_t length, uint8_t extensionId,
                                std::string& extensionName, std::vector<uint8_t>& payload);

private:
    std::unordered_map<std::string, uint8_t> m_peerExtensions;  ///< Extensions supported by the peer
    std::unordered_map<std::string, uint8_t> m_localExtensions; ///< Extensions supported locally
    uint32_t m_metadataSize;                                    ///< Size of the metadata
    std::string m_clientVersion;                                ///< Peer's client version
    uint16_t m_peerPort;                                        ///< Peer's TCP port
};

} // namespace dht_hunter::metadata
