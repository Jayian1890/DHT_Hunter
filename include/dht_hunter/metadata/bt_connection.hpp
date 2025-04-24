#pragma once

#include "dht_hunter/network/socket.hpp"
#include "dht_hunter/network/async_socket.hpp"
#include "dht_hunter/network/end_point.hpp"
#include "dht_hunter/metadata/bt_handshake.hpp"
#include "dht_hunter/metadata/extension_protocol.hpp"
#include "dht_hunter/metadata/ut_metadata.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dht_hunter::metadata {

/**
 * @brief Size of the receive buffer
 */
constexpr size_t BT_RECEIVE_BUFFER_SIZE = 64 * 1024;

/**
 * @enum BTConnectionState
 * @brief States of a BitTorrent connection
 */
enum class BTConnectionState {
    Disconnected,      ///< Not connected
    Connecting,        ///< Connection in progress
    HandshakeSent,     ///< Handshake sent, waiting for response
    HandshakeReceived, ///< Handshake received, waiting for extension handshake
    ExtensionHandshakeSent,     ///< Extension handshake sent
    ExtensionHandshakeReceived, ///< Extension handshake received
    Connected,         ///< Fully connected and ready for metadata exchange
    Disconnecting,     ///< Disconnection in progress
    Error              ///< Error state
};

/**
 * @brief Callback for connection state changes
 * @param state The new connection state
 * @param error The error message (if any)
 */
using BTConnectionStateCallback = std::function<void(BTConnectionState state, const std::string& error)>;

/**
 * @brief Callback for metadata completion
 * @param metadata The complete metadata
 * @param size The size of the metadata
 */
using BTMetadataCallback = std::function<void(const std::vector<uint8_t>& metadata, uint32_t size)>;

/**
 * @class BTConnection
 * @brief Manages a BitTorrent connection for metadata exchange
 */
class BTConnection {
public:
    /**
     * @brief Constructor
     * @param socket The async socket to use
     * @param infoHash The info hash of the torrent
     * @param stateCallback Callback for connection state changes
     * @param metadataCallback Callback for metadata completion
     */
    BTConnection(std::unique_ptr<network::AsyncSocket> socket,
                const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash,
                BTConnectionStateCallback stateCallback,
                BTMetadataCallback metadataCallback);

    /**
     * @brief Destructor
     */
    ~BTConnection();

    /**
     * @brief Connects to a remote peer
     * @param endpoint The remote endpoint
     * @return True if the connection was initiated successfully, false otherwise
     */
    bool connect(const network::EndPoint& endpoint);

    /**
     * @brief Disconnects from the peer
     */
    void disconnect();

    /**
     * @brief Gets the current connection state
     * @return The connection state
     */
    BTConnectionState getState() const;

    /**
     * @brief Gets the info hash
     * @return The info hash
     */
    const std::array<uint8_t, BT_INFOHASH_LENGTH>& getInfoHash() const;

    /**
     * @brief Gets the peer ID
     * @return The peer ID
     */
    const std::array<uint8_t, BT_PEER_ID_LENGTH>& getPeerId() const;

    /**
     * @brief Gets the remote endpoint
     * @return The remote endpoint
     */
    const network::EndPoint& getRemoteEndpoint() const;

    /**
     * @brief Gets the client version of the peer
     * @return The client version string
     */
    std::string getPeerClientVersion() const;

private:
    /**
     * @brief Sends the BitTorrent handshake
     */
    void sendHandshake();

    /**
     * @brief Processes a received handshake
     * @param data The handshake data
     * @param length The handshake length
     */
    void processHandshake(const uint8_t* data, size_t length);

    /**
     * @brief Sends the extension protocol handshake
     */
    void sendExtensionHandshake();

    /**
     * @brief Processes a received message
     * @param data The message data
     * @param length The message length
     */
    void processMessage(const uint8_t* data, size_t length);

    /**
     * @brief Processes a received extension message
     * @param data The message data
     * @param length The message length
     */
    void processExtensionMessage(const uint8_t* data, size_t length);

    /**
     * @brief Requests metadata pieces
     */
    void requestMetadataPieces();

    /**
     * @brief Handles a metadata piece
     * @param piece The piece index
     * @param data The piece data
     * @param size The piece size
     * @param totalSize The total metadata size
     */
    void handleMetadataPiece(uint32_t piece, const uint8_t* data, uint32_t size, uint32_t totalSize);

    /**
     * @brief Handles a metadata request
     * @param piece The piece index
     * @return The piece data, or empty vector if the piece is not available
     */
    std::vector<uint8_t> handleMetadataRequest(uint32_t piece);

    /**
     * @brief Handles a metadata rejection
     * @param piece The piece index
     */
    void handleMetadataReject(uint32_t piece);

    /**
     * @brief Checks if all metadata pieces have been received
     * @return True if all pieces have been received, false otherwise
     */
    bool isMetadataComplete() const;

    /**
     * @brief Assembles the complete metadata from the received pieces
     * @return The complete metadata
     */
    std::vector<uint8_t> assembleMetadata() const;

    /**
     * @brief Validates the metadata against the info hash
     * @param metadata The metadata to validate
     * @return True if the metadata is valid, false otherwise
     */
    bool validateMetadata(const std::vector<uint8_t>& metadata) const;

    /**
     * @brief Sets the connection state
     * @param state The new state
     * @param error The error message (if any)
     */
    void setState(BTConnectionState state, const std::string& error = "");

    /**
     * @brief Callback for async connect operations
     * @param socket The socket
     * @param error The error code
     */
    void onConnect(network::Socket* socket, network::SocketError error);

    /**
     * @brief Callback for async read operations
     * @param socket The socket
     * @param data The data that was read
     * @param size The number of bytes read
     * @param error The error code
     */
    void onRead(network::Socket* socket, const uint8_t* data, int size, network::SocketError error);

    /**
     * @brief Callback for async write operations
     * @param socket The socket
     * @param size The number of bytes written
     * @param error The error code
     */
    void onWrite(network::Socket* socket, int size, network::SocketError error);

    std::unique_ptr<network::AsyncSocket> m_socket;  ///< The async socket
    BTConnectionState m_state;                      ///< Current connection state
    std::array<uint8_t, BT_INFOHASH_LENGTH> m_infoHash; ///< Info hash of the torrent
    std::array<uint8_t, BT_PEER_ID_LENGTH> m_peerId;    ///< Local peer ID
    network::EndPoint m_remoteEndpoint;             ///< Remote endpoint
    BTConnectionStateCallback m_stateCallback;      ///< Callback for state changes
    BTMetadataCallback m_metadataCallback;          ///< Callback for metadata completion
    
    ExtensionProtocol m_extensionProtocol;          ///< Extension protocol handler
    UTMetadata m_utMetadata;                        ///< ut_metadata extension handler
    
    std::vector<uint8_t> m_receiveBuffer;           ///< Buffer for received data
    size_t m_receiveBufferPos;                      ///< Current position in the receive buffer
    
    std::vector<std::vector<uint8_t>> m_metadataPieces; ///< Received metadata pieces
    std::vector<bool> m_metadataPiecesReceived;     ///< Flags for received pieces
    uint32_t m_metadataSize;                        ///< Total metadata size
    uint32_t m_metadataPiecesRequested;             ///< Number of pieces requested
    uint32_t m_metadataPiecesReceived;              ///< Number of pieces received
};

} // namespace dht_hunter::metadata
