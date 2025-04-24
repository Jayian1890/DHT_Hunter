#include "dht_hunter/metadata/bt_connection.hpp"
#include "dht_hunter/logforge/logforge.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>

namespace dht_hunter::metadata {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("BTConnection");
    
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
    
    // Client version string
    constexpr const char* CLIENT_VERSION = "DHT-Hunter 0.1.0";
    
    // Maximum number of metadata pieces to request at once
    constexpr uint32_t MAX_METADATA_REQUESTS = 5;
    
    // Convert bytes to hex string
    std::string bytesToHex(const uint8_t* data, size_t length) {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (size_t i = 0; i < length; ++i) {
            ss << std::setw(2) << static_cast<int>(data[i]);
        }
        return ss.str();
    }
}

BTConnection::BTConnection(std::unique_ptr<network::AsyncSocket> socket,
                         const std::array<uint8_t, BT_INFOHASH_LENGTH>& infoHash,
                         BTConnectionStateCallback stateCallback,
                         BTMetadataCallback metadataCallback)
    : m_socket(std::move(socket)),
      m_state(BTConnectionState::Disconnected),
      m_infoHash(infoHash),
      m_stateCallback(stateCallback),
      m_metadataCallback(metadataCallback),
      m_receiveBuffer(BT_RECEIVE_BUFFER_SIZE),
      m_receiveBufferPos(0),
      m_metadataSize(0),
      m_metadataPiecesRequested(0),
      m_metadataPiecesReceived(0) {
    
    // Generate a peer ID
    m_peerId = generatePeerId();
    
    logger->debug("Created BTConnection for info hash: {}", 
                 bytesToHex(m_infoHash.data(), m_infoHash.size()));
}

BTConnection::~BTConnection() {
    disconnect();
}

bool BTConnection::connect(const network::EndPoint& endpoint) {
    if (m_state != BTConnectionState::Disconnected) {
        logger->warning("Cannot connect: already connected or connecting");
        return false;
    }
    
    m_remoteEndpoint = endpoint;
    
    logger->debug("Connecting to {}:{}", endpoint.getAddress().toString(), endpoint.getPort());
    
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
    
    logger->debug("Disconnecting from {}:{}", 
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
    
    logger->debug("Sending handshake to {}:{}", 
                 m_remoteEndpoint.getAddress().toString(), 
                 m_remoteEndpoint.getPort());
    
    // Send the handshake
    m_socket->asyncWrite(handshakeData.data(), handshakeData.size(),
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
    
    logger->debug("Received valid handshake from {}:{}", 
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
    auto handshakeData = m_extensionProtocol.createHandshake(0, CLIENT_VERSION);
    
    logger->debug("Sending extension handshake to {}:{}", 
                 m_remoteEndpoint.getAddress().toString(), 
                 m_remoteEndpoint.getPort());
    
    // Send the handshake
    m_socket->asyncWrite(handshakeData.data(), handshakeData.size(),
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
            logger->debug("Received choke message");
            break;
            
        case BT_MESSAGE_UNCHOKE:
            logger->debug("Received unchoke message");
            break;
            
        case BT_MESSAGE_INTERESTED:
            logger->debug("Received interested message");
            break;
            
        case BT_MESSAGE_NOT_INTERESTED:
            logger->debug("Received not interested message");
            break;
            
        case BT_MESSAGE_HAVE:
            logger->debug("Received have message");
            break;
            
        case BT_MESSAGE_BITFIELD:
            logger->debug("Received bitfield message");
            break;
            
        case BT_MESSAGE_REQUEST:
            logger->debug("Received request message");
            break;
            
        case BT_MESSAGE_PIECE:
            logger->debug("Received piece message");
            break;
            
        case BT_MESSAGE_CANCEL:
            logger->debug("Received cancel message");
            break;
            
        default:
            logger->warning("Received unknown message ID: {}", messageId);
            break;
    }
}

void BTConnection::processExtensionMessage(const uint8_t* data, size_t length) {
    if (length < 2) {
        logger->error("Invalid extension message: too short");
        return;
    }
    
    // Get the extension ID
    uint8_t extensionId = data[1];
    
    if (extensionId == BT_EXTENSION_HANDSHAKE_ID) {
        // Extension handshake
        logger->debug("Received extension handshake");
        
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
                    
                    logger->debug("Metadata size: {} bytes, {} pieces", 
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
                logger->debug("Received extension message for {}", extensionName);
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
                logger->debug("Requesting metadata piece {}", i);
                
                // Send the message
                m_socket->asyncWrite(extensionMessage.data(), extensionMessage.size(),
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
    logger->debug("Received metadata piece {}", piece);
    
    // Update metadata size if needed
    if (m_metadataSize == 0) {
        m_metadataSize = totalSize;
        m_utMetadata.setMetadataSize(totalSize);
        
        // Initialize metadata pieces tracking
        uint32_t numPieces = m_utMetadata.getNumPieces();
        m_metadataPieces.resize(numPieces);
        m_metadataPiecesReceived.resize(numPieces, false);
        
        logger->debug("Updated metadata size: {} bytes, {} pieces", 
                     totalSize, numPieces);
    }
    
    // Store the piece
    if (piece < m_metadataPieces.size()) {
        m_metadataPieces[piece].assign(data, data + size);
        m_metadataPiecesReceived[piece] = true;
        m_metadataPiecesReceived++;
        
        logger->debug("Stored metadata piece {} ({} bytes)", piece, size);
        
        // Check if we have all pieces
        if (isMetadataComplete()) {
            logger->debug("All metadata pieces received");
            
            // Assemble the metadata
            auto metadata = assembleMetadata();
            
            // Validate the metadata
            if (validateMetadata(metadata)) {
                logger->debug("Metadata validation successful");
                
                // Notify the callback
                if (m_metadataCallback) {
                    m_metadataCallback(metadata, m_metadataSize);
                }
                
                // Disconnect
                disconnect();
            } else {
                logger->error("Metadata validation failed");
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
    // We don't have the metadata, so reject all requests
    return {};
}

void BTConnection::handleMetadataReject(uint32_t piece) {
    logger->debug("Metadata piece {} rejected", piece);
    
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
    uint8_t hash[SHA_DIGEST_LENGTH];
    SHA1(metadata.data(), metadata.size(), hash);
    
    // Compare with the info hash
    return std::memcmp(hash, m_infoHash.data(), BT_INFOHASH_LENGTH) == 0;
}

void BTConnection::setState(BTConnectionState state, const std::string& error) {
    if (m_state != state) {
        m_state = state;
        
        if (!error.empty()) {
            logger->error("Connection state changed to {}: {}", 
                         static_cast<int>(state), error);
        } else {
            logger->debug("Connection state changed to {}", 
                         static_cast<int>(state));
        }
        
        // Notify the callback
        if (m_stateCallback) {
            m_stateCallback(state, error);
        }
    }
}

void BTConnection::onConnect(network::Socket* socket, network::SocketError error) {
    if (error != network::SocketError::None) {
        setState(BTConnectionState::Error, 
                "Connection failed: " + network::Socket::getErrorString(error));
        return;
    }
    
    logger->debug("Connected to {}:{}", 
                 m_remoteEndpoint.getAddress().toString(), 
                 m_remoteEndpoint.getPort());
    
    // Send the handshake
    sendHandshake();
}

void BTConnection::onRead(network::Socket* socket, const uint8_t* data, int size, network::SocketError error) {
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
    if (m_receiveBufferPos + size > m_receiveBuffer.size()) {
        // Buffer overflow, resize the buffer
        m_receiveBuffer.resize(m_receiveBufferPos + size);
    }
    
    std::memcpy(m_receiveBuffer.data() + m_receiveBufferPos, data, size);
    m_receiveBufferPos += size;
    
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
