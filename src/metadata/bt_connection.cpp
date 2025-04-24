#include "dht_hunter/metadata/bt_connection.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include "dht_hunter/util/hash.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <iomanip>

DEFINE_COMPONENT_LOGGER("Metadata", "BTConnection")

namespace dht_hunter::metadata {

namespace {

    // BitTorrent message IDs
    constexpr uint8_t BT_MESSAGE_CHOKE = 0;
    constexpr uint8_t BT_MESSAGE_UNCHOKE = 1;
    constexpr uint8_t BT_MESSAGE_INTERESTED = 2;
    constexpr uint8_t BT_MESSAGE_NOT_INTERESTED = 3;
    constexpr uint8_t BT_MESSAGE_HAVE = 4;
    constexpr uint8_t BT_MESSAGE_BITFIELD = 5;
    constexpr uint8_t BT_MESSAGE_REQUEST = 6;
    constexpr uint8_t BT_MESSAGE_PIECE = 7;
    constexpr uint8_t BT_MESSAGE_CANCEL = 8;

    // Maximum number of metadata pieces to request at once
    constexpr uint32_t MAX_METADATA_REQUESTS = 5;

    // Use the utility function for bytesToHex
}

BTConnection::BTConnection(std::unique_ptr<network::AsyncSocket> socket,
                         const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash,
                         BTConnectionStateCallback stateCallback,
                         BTMetadataCallback metadataCallback,
                         const std::string& peerIdPrefix,
                         const std::string& clientVersion)
    : m_socket(std::move(socket)),
      m_state(BTConnectionState::Disconnected),
      m_infoHash(infoHash),
      m_stateCallback(stateCallback),
      m_metadataCallback(metadataCallback),
      m_clientVersion(clientVersion),
      m_receiveBuffer(BT_RECEIVE_BUFFER_SIZE),
      m_receiveBufferPos(0),
      m_metadataSize(0),
      m_metadataPiecesRequested(0),
      m_receivedPieceCount(0) {

    // Generate a peer ID with the specified prefix
    m_peerId = generatePeerId(peerIdPrefix);

    getLogger()->debug("Created BTConnection for info hash: {} with peer ID prefix: {}",
                 util::bytesToHex(m_infoHash.data(), m_infoHash.size()),
                 peerIdPrefix);
}

BTConnection::~BTConnection() {
    disconnect();
}

bool BTConnection::connect(const network::EndPoint& endpoint) {
    if (m_state != BTConnectionState::Disconnected) {
        getLogger()->warning("Cannot connect: already connected or connecting");
        return false;
    }

    m_remoteEndpoint = endpoint;

    getLogger()->debug("Connecting to {}:{}", endpoint.getAddress().toString(), endpoint.getPort());

    setState(BTConnectionState::Connecting);

    // Start the connection
    if (!m_socket->asyncConnect(endpoint,
                               [this](network::Socket* socket, network::SocketError error) {
                                   this->onConnect(socket, error);
                               })) {
        setState(BTConnectionState::Error, "Failed to initiate connection");
        return false;
    }

    return true;
}

void BTConnection::disconnect() {
    if (m_state == BTConnectionState::Disconnected) {
        return;
    }

    getLogger()->debug("Disconnecting from {}:{}",
                 m_remoteEndpoint.getAddress().toString(),
                 m_remoteEndpoint.getPort());

    setState(BTConnectionState::Disconnecting);

    // Close the socket
    if (m_socket && m_socket->getSocket()) {
        m_socket->getSocket()->close();
    }

    setState(BTConnectionState::Disconnected);
}

BTConnectionState BTConnection::getState() const {
    return m_state;
}

const std::array<uint8_t, BT_INFOHASH_LENGTH>& BTConnection::getInfoHash() const {
    return m_infoHash;
}

const std::array<uint8_t, BT_PEER_ID_LENGTH>& BTConnection::getPeerId() const {
    return m_peerId;
}

const network::EndPoint& BTConnection::getRemoteEndpoint() const {
    return m_remoteEndpoint;
}

std::string BTConnection::getPeerClientVersion() const {
    return m_extensionProtocol.getClientVersion();
}

void BTConnection::sendHandshake() {
    // Create the handshake
    BTHandshake handshake(m_infoHash, m_peerId, true);

    // Serialize the handshake
    auto handshakeData = handshake.serialize();

    getLogger()->debug("Sending handshake to {}:{}",
                 m_remoteEndpoint.getAddress().toString(),
                 m_remoteEndpoint.getPort());

    // Send the handshake
    m_socket->asyncWrite(handshakeData.data(), static_cast<int>(handshakeData.size()),
                        [this](network::Socket* socket, int size, network::SocketError error) {
                            this->onWrite(socket, size, error);
                        });

    setState(BTConnectionState::HandshakeSent);

    // Start reading the response
    m_socket->asyncRead(m_receiveBuffer.data(), m_receiveBuffer.size(),
                       [this](network::Socket* socket, const uint8_t* data, int size, network::SocketError error) {
                           this->onRead(socket, data, size, error);
                       });
}

void BTConnection::processHandshake(const uint8_t* data, size_t length) {
    // Parse the handshake
    BTHandshake handshake;
    if (!handshake.deserialize(data, length)) {
        setState(BTConnectionState::Error, "Invalid handshake received");
        disconnect();
        return;
    }

    // Verify the info hash
    if (std::memcmp(handshake.infoHash.data(), m_infoHash.data(), BT_INFOHASH_LENGTH) != 0) {
        setState(BTConnectionState::Error, "Info hash mismatch in handshake");
        disconnect();
        return;
    }

    getLogger()->debug("Received valid handshake from {}:{}",
                 m_remoteEndpoint.getAddress().toString(),
                 m_remoteEndpoint.getPort());

    // Check if the peer supports the extension protocol
    if (!handshake.supportsExtensionProtocol()) {
        setState(BTConnectionState::Error, "Peer does not support extension protocol");
        disconnect();
        return;
    }

    setState(BTConnectionState::HandshakeReceived);

    // Send the extension handshake
    sendExtensionHandshake();
}

void BTConnection::sendExtensionHandshake() {
    // Create the extension handshake
    auto handshakeData = m_extensionProtocol.createHandshake(0, m_clientVersion);

    getLogger()->debug("Sending extension handshake to {}:{} with client version: {}",
                 m_remoteEndpoint.getAddress().toString(),
                 m_remoteEndpoint.getPort(),
                 m_clientVersion);

    // Send the handshake
    m_socket->asyncWrite(handshakeData.data(), static_cast<int>(handshakeData.size()),
                        [this](network::Socket* socket, int size, network::SocketError error) {
                            this->onWrite(socket, size, error);
                        });

    setState(BTConnectionState::ExtensionHandshakeSent);
}

void BTConnection::processMessage(const uint8_t* data, size_t length) {
    if (length == 0) {
        // Keep-alive message
        return;
    }

    // Get the message ID
    uint8_t messageId = data[0];

    // Process based on message ID
    switch (messageId) {
        case BT_EXTENSION_MESSAGE_ID:
            // Extension message
            if (length >= 2) {
                processExtensionMessage(data, length);
            }
            break;

        case BT_MESSAGE_CHOKE:
            getLogger()->debug("Received choke message");
            break;

        case BT_MESSAGE_UNCHOKE:
            getLogger()->debug("Received unchoke message");
            break;

        case BT_MESSAGE_INTERESTED:
            getLogger()->debug("Received interested message");
            break;

        case BT_MESSAGE_NOT_INTERESTED:
            getLogger()->debug("Received not interested message");
            break;

        case BT_MESSAGE_HAVE:
            getLogger()->debug("Received have message");
            break;

        case BT_MESSAGE_BITFIELD:
            getLogger()->debug("Received bitfield message");
            break;

        case BT_MESSAGE_REQUEST:
            getLogger()->debug("Received request message");
            break;

        case BT_MESSAGE_PIECE:
            getLogger()->debug("Received piece message");
            break;

        case BT_MESSAGE_CANCEL:
            getLogger()->debug("Received cancel message");
            break;

        default:
            getLogger()->warning("Received unknown message ID: {}", messageId);
            break;
    }
}

void BTConnection::processExtensionMessage(const uint8_t* data, size_t length) {
    if (length < 2) {
        getLogger()->error("Invalid extension message: too short");
        return;
    }

    // Get the extension ID
    uint8_t extensionId = data[1];

    if (extensionId == BT_EXTENSION_HANDSHAKE_ID) {
        // Extension handshake
        getLogger()->debug("Received extension handshake");

        if (m_extensionProtocol.processHandshake(data, length)) {
            setState(BTConnectionState::ExtensionHandshakeReceived);

            // Check if the peer supports ut_metadata
            if (m_extensionProtocol.supportsExtension(UT_METADATA_EXTENSION_NAME)) {
                // Get the metadata size
                uint32_t metadataSize = m_extensionProtocol.getMetadataSize();
                if (metadataSize > 0) {
                    m_metadataSize = metadataSize;
                    m_utMetadata.setMetadataSize(metadataSize);

                    // Initialize metadata pieces tracking
                    uint32_t numPieces = m_utMetadata.getNumPieces();
                    m_metadataPieces.resize(numPieces);
                    m_metadataPiecesReceived.resize(numPieces, false);

                    getLogger()->debug("Metadata size: {} bytes, {} pieces",
                                 metadataSize, numPieces);

                    setState(BTConnectionState::Connected);

                    // Start requesting metadata pieces
                    requestMetadataPieces();
                } else {
                    setState(BTConnectionState::Error, "Peer did not provide metadata size");
                    disconnect();
                }
            } else {
                setState(BTConnectionState::Error, "Peer does not support ut_metadata extension");
                disconnect();
            }
        } else {
            setState(BTConnectionState::Error, "Failed to process extension handshake");
            disconnect();
        }
    } else {
        // Other extension message
        std::string extensionName;
        std::vector<uint8_t> payload;

        if (m_extensionProtocol.processExtensionMessage(data, length, extensionId,
                                                      extensionName, payload)) {
            if (extensionName == UT_METADATA_EXTENSION_NAME) {
                // ut_metadata message
                m_utMetadata.processMessage(
                    payload.data(), payload.size(),
                    [this](uint32_t piece) { return this->handleMetadataRequest(piece); },
                    [this](uint32_t piece, const uint8_t* data, uint32_t size, uint32_t totalSize) {
                        this->handleMetadataPiece(piece, data, size, totalSize);
                    },
                    [this](uint32_t piece) { this->handleMetadataReject(piece); }
                );
            } else {
                getLogger()->debug("Received extension message for {}", extensionName);
            }
        }
    }
}

void BTConnection::requestMetadataPieces() {
    if (m_state != BTConnectionState::Connected) {
        return;
    }

    uint32_t numPieces = m_utMetadata.getNumPieces();
    if (numPieces == 0) {
        return;
    }

    // Request pieces that haven't been received yet
    uint32_t requested = 0;
    for (uint32_t i = 0; i < numPieces && requested < MAX_METADATA_REQUESTS; ++i) {
        if (!m_metadataPiecesReceived[i]) {
            // Create the request message
            auto requestMessage = m_utMetadata.createRequestMessage(i);

            // Create the extension message
            auto extensionMessage = m_extensionProtocol.createExtensionMessage(
                UT_METADATA_EXTENSION_NAME, requestMessage);

            if (!extensionMessage.empty()) {
                getLogger()->debug("Requesting metadata piece {}", i);

                // Send the message
                m_socket->asyncWrite(extensionMessage.data(), static_cast<int>(extensionMessage.size()),
                                    [this](network::Socket* socket, int size, network::SocketError error) {
                                        this->onWrite(socket, size, error);
                                    });

                requested++;
                m_metadataPiecesRequested++;
            }
        }
    }
}

void BTConnection::handleMetadataPiece(uint32_t piece, const uint8_t* data, uint32_t size, uint32_t totalSize) {
    getLogger()->debug("Received metadata piece {}", piece);

    // Update metadata size if needed
    if (m_metadataSize == 0) {
        m_metadataSize = totalSize;
        m_utMetadata.setMetadataSize(totalSize);

        // Initialize metadata pieces tracking
        uint32_t numPieces = m_utMetadata.getNumPieces();
        m_metadataPieces.resize(numPieces);
        m_metadataPiecesReceived.resize(numPieces, false);

        getLogger()->debug("Updated metadata size: {} bytes, {} pieces",
                     totalSize, numPieces);
    }

    // Store the piece
    if (piece < m_metadataPieces.size()) {
        m_metadataPieces[piece].assign(data, data + size);
        m_metadataPiecesReceived[piece] = true;
        m_receivedPieceCount++;

        getLogger()->debug("Stored metadata piece {} ({} bytes)", piece, size);

        // Check if we have all pieces
        if (isMetadataComplete()) {
            getLogger()->debug("All metadata pieces received");

            // Assemble the metadata
            auto metadata = assembleMetadata();

            // Validate the metadata
            if (validateMetadata(metadata)) {
                getLogger()->debug("Metadata validation successful");

                // Notify the callback
                if (m_metadataCallback) {
                    m_metadataCallback(metadata, m_metadataSize);
                }

                // Disconnect
                disconnect();
            } else {
                getLogger()->error("Metadata validation failed");
                setState(BTConnectionState::Error, "Metadata validation failed");
                disconnect();
            }
        } else {
            // Request more pieces
            requestMetadataPieces();
        }
    }
}

std::vector<uint8_t> BTConnection::handleMetadataRequest(uint32_t piece) {
    (void)piece; // Unused parameter
    // We don't have the metadata, so reject all requests
    return {};
}

void BTConnection::handleMetadataReject(uint32_t piece) {
    getLogger()->debug("Metadata piece {} rejected", piece);

    // Try requesting a different piece
    requestMetadataPieces();
}

bool BTConnection::isMetadataComplete() const {
    if (m_metadataPieces.empty()) {
        return false;
    }

    for (bool received : m_metadataPiecesReceived) {
        if (!received) {
            return false;
        }
    }

    return true;
}

std::vector<uint8_t> BTConnection::assembleMetadata() const {
    std::vector<uint8_t> metadata;
    metadata.reserve(m_metadataSize);

    for (const auto& piece : m_metadataPieces) {
        metadata.insert(metadata.end(), piece.begin(), piece.end());
    }

    // Trim to the correct size
    if (metadata.size() > m_metadataSize) {
        metadata.resize(m_metadataSize);
    }

    return metadata;
}

bool BTConnection::validateMetadata(const std::vector<uint8_t>& metadata) const {
    // Compute the SHA-1 hash of the metadata
    std::array<uint8_t, 20> hash = util::sha1(metadata);

    // Compare with the info hash
    return std::memcmp(hash.data(), m_infoHash.data(), BT_INFOHASH_LENGTH) == 0;
}

void BTConnection::setState(BTConnectionState state, const std::string& error) {
    if (m_state != state) {
        m_state = state;

        if (!error.empty()) {
            getLogger()->error("Connection state changed to {}: {}",
                         static_cast<int>(state), error);
        } else {
            getLogger()->debug("Connection state changed to {}",
                         static_cast<int>(state));
        }

        // Notify the callback
        if (m_stateCallback) {
            m_stateCallback(state, error);
        }
    }
}

void BTConnection::onConnect(network::Socket* socket, network::SocketError error) {
    (void)socket; // Unused parameter
    if (error != network::SocketError::None) {
        setState(BTConnectionState::Error,
                "Connection failed: " + network::Socket::getErrorString(error));
        return;
    }

    getLogger()->debug("Connected to {}:{}",
                 m_remoteEndpoint.getAddress().toString(),
                 m_remoteEndpoint.getPort());

    // Send the handshake
    sendHandshake();
}

void BTConnection::onRead(network::Socket* socket, const uint8_t* data, int size, network::SocketError error) {
    (void)socket; // Unused parameter
    if (error != network::SocketError::None) {
        setState(BTConnectionState::Error,
                "Read failed: " + network::Socket::getErrorString(error));
        disconnect();
        return;
    }

    if (size <= 0) {
        setState(BTConnectionState::Error, "Connection closed by peer");
        disconnect();
        return;
    }

    // Copy the data to the receive buffer
    if (m_receiveBufferPos + static_cast<size_t>(size) > m_receiveBuffer.size()) {
        // Buffer overflow, resize the buffer
        m_receiveBuffer.resize(m_receiveBufferPos + static_cast<size_t>(size));
    }

    std::memcpy(m_receiveBuffer.data() + m_receiveBufferPos, data, static_cast<size_t>(size));
    m_receiveBufferPos += static_cast<size_t>(size);

    // Process the received data
    size_t processed = 0;

    if (m_state == BTConnectionState::HandshakeSent) {
        // Expecting a handshake
        if (m_receiveBufferPos >= BT_HANDSHAKE_LENGTH) {
            processHandshake(m_receiveBuffer.data(), BT_HANDSHAKE_LENGTH);
            processed = BT_HANDSHAKE_LENGTH;
        }
    } else {
        // Expecting BitTorrent messages
        while (m_receiveBufferPos - processed >= 4) {
            // Get the message length
            uint32_t messageLength = 0;
            std::memcpy(&messageLength, m_receiveBuffer.data() + processed, 4);
            messageLength = ntohl(messageLength);

            // Check if we have the complete message
            if (m_receiveBufferPos - processed >= 4 + messageLength) {
                // Process the message
                if (messageLength > 0) {
                    processMessage(m_receiveBuffer.data() + processed + 4, messageLength);
                }

                processed += 4 + messageLength;
            } else {
                break;
            }
        }
    }

    // Remove processed data from the buffer
    if (processed > 0) {
        if (processed < m_receiveBufferPos) {
            std::memmove(m_receiveBuffer.data(),
                        m_receiveBuffer.data() + processed,
                        m_receiveBufferPos - processed);
            m_receiveBufferPos -= processed;
        } else {
            m_receiveBufferPos = 0;
        }
    }

    // Continue reading
    m_socket->asyncRead(m_receiveBuffer.data() + m_receiveBufferPos,
                       m_receiveBuffer.size() - m_receiveBufferPos,
                       [this](network::Socket* socket, const uint8_t* data, int size, network::SocketError error) {
                           this->onRead(socket, data, size, error);
                       });
}

void BTConnection::onWrite(network::Socket* socket, int size, network::SocketError error) {
    (void)socket; // Unused parameter
    if (error != network::SocketError::None) {
        setState(BTConnectionState::Error,
                "Write failed: " + network::Socket::getErrorString(error));
        disconnect();
        return;
    }

    if (size <= 0) {
        setState(BTConnectionState::Error, "Connection closed by peer");
        disconnect();
        return;
    }
}

} // namespace dht_hunter::metadata
