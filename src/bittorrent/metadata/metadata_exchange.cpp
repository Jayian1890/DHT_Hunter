#include "dht_hunter/bittorrent/metadata/metadata_exchange.hpp"
#include "dht_hunter/types/info_hash_metadata.hpp"
#include "dht_hunter/utility/metadata/metadata_utils.hpp"
#include "dht_hunter/utility/string/string_utils.hpp"
#include "dht_hunter/utility/hash/hash_utils.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace dht_hunter::bittorrent::metadata {

MetadataExchange::MetadataExchange() {
    // Start the cleanup thread
    m_cleanupThread = std::thread(&MetadataExchange::cleanupTimedOutConnectionsPeriodically, this);
}

MetadataExchange::~MetadataExchange() {
    // Stop the cleanup thread
    m_running = false;
    m_cleanupCondition.notify_all();

    // Close all connections
    {
        std::lock_guard<std::mutex> lock(m_connectionsMutex);
        for (auto& [_, connection] : m_connections) {
            connection->client.disconnect();
        }
        m_connections.clear();
    }

    // Join the cleanup thread after closing connections
    if (m_cleanupThread.joinable()) {
        m_cleanupThread.join();
    }
}

bool MetadataExchange::acquireMetadata(
    const types::InfoHash& infoHash,
    const std::vector<network::EndPoint>& peers,
    std::function<void(bool success)> callback,
    int maxConcurrentPeers) {

    if (peers.empty()) {
        unified_event::logWarning("BitTorrent.MetadataExchange", "No peers provided for info hash: " + types::infoHashToString(infoHash));
        return false;
    }

    // Check if we already have metadata for this info hash
    auto metadata = utility::metadata::MetadataUtils::getInfoHashMetadata(infoHash);
    if (metadata && !metadata->getName().empty() && metadata->getTotalSize() > 0) {
        unified_event::logInfo("BitTorrent.MetadataExchange", "Already have metadata for info hash: " + types::infoHashToString(infoHash));
        callback(true);
        return true;
    }

    // Check if we already have a task for this info hash
    std::string taskId = types::infoHashToString(infoHash);
    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        auto it = m_tasks.find(taskId);
        if (it != m_tasks.end()) {
            // Task already exists, just update the callback
            it->second->callback = callback;
            unified_event::logInfo("BitTorrent.MetadataExchange", "Metadata acquisition already in progress for info hash: " + types::infoHashToString(infoHash));
            return true;
        }
    }

    // Limit the number of concurrent peers
    maxConcurrentPeers = std::min(maxConcurrentPeers, MAX_CONCURRENT_PEERS_PER_INFOHASH);

    // Create a new task
    auto task = std::make_shared<AcquisitionTask>(infoHash, peers, callback, maxConcurrentPeers);

    // Add the task to the active tasks
    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        m_tasks[taskId] = task;
    }

    // Start connecting to peers
    tryMorePeers(task);

    return true;
}

void MetadataExchange::cancelAcquisition(const types::InfoHash& infoHash) {
    std::string taskId = types::infoHashToString(infoHash);

    // Remove the task
    std::shared_ptr<AcquisitionTask> task;
    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        auto it = m_tasks.find(taskId);
        if (it != m_tasks.end()) {
            task = it->second;
            m_tasks.erase(it);
        }
    }

    if (!task) {
        return;
    }

    // Mark the task as completed
    task->completed = true;

    // Disconnect all connections for this task
    std::vector<std::string> connectionsToRemove;
    {
        std::lock_guard<std::mutex> lock(m_connectionsMutex);
        for (auto& [id, connection] : m_connections) {
            if (connection->infoHash == infoHash) {
                connection->client.disconnect();
                connectionsToRemove.push_back(id);
            }
        }

        // Remove the connections
        for (const auto& id : connectionsToRemove) {
            m_connections.erase(id);
        }
    }

    unified_event::logInfo("BitTorrent.MetadataExchange", "Cancelled metadata acquisition for info hash: " + types::infoHashToString(infoHash));
}

bool MetadataExchange::retryConnection(std::shared_ptr<MetadataExchange::PeerConnection> connection) {
    // Check if the connection has exceeded the maximum retry count
    if (connection->retryCount >= MAX_RETRY_COUNT) {
        unified_event::logWarning("BitTorrent.MetadataExchange", "Maximum retry count reached for peer: " + connection->peerId);
        return false;
    }

    // Increment the retry count
    connection->retryCount++;

    // Reset connection state
    connection->handshakeComplete = false;
    connection->extensionHandshakeComplete = false;
    connection->metadataExchangeComplete = false;
    connection->connected = false;
    connection->failed = false;
    connection->buffer.clear();
    connection->metadataPieces.clear();
    connection->startTime = std::chrono::steady_clock::now();

    // Try to connect again
    unified_event::logInfo("BitTorrent.MetadataExchange", "Retrying connection to peer: " + connection->peerId + " (attempt " + std::to_string(connection->retryCount) + ")");
    return connectToPeer(connection->peer, connection);
}

void MetadataExchange::tryMorePeers(std::shared_ptr<AcquisitionTask> task) {
    // Check if the task is completed
    if (task->completed) {
        return;
    }

    // Check if we've reached the maximum number of concurrent connections
    if (task->activeConnections >= task->maxConcurrentPeers) {
        return;
    }

    // Calculate how many more connections we can make
    int availableSlots = task->maxConcurrentPeers - task->activeConnections;
    if (availableSlots <= 0) {
        return;
    }

    // Try to connect to more peers
    int connectedCount = 0;

    for (int i = 0; i < availableSlots && !task->remainingPeers.empty(); i++) {
        // Get the next peer
        auto peer = task->remainingPeers.back();
        task->remainingPeers.pop_back();

        // Create a new connection
        auto connection = std::make_shared<PeerConnection>(task->infoHash, peer);

        // Try to connect
        if (connectToPeer(peer, connection)) {
            // Add the connection to the active connections
            std::string connectionId = types::infoHashToString(task->infoHash) + "_" + peer.toString();
            std::lock_guard<std::mutex> lock(m_connectionsMutex);
            m_connections[connectionId] = connection;

            // Increment the active connections count
            task->activeConnections++;
            connectedCount++;
        }
    }

    // If we couldn't connect to any peers and there are no active connections, fail the task
    if (connectedCount == 0 && task->activeConnections == 0 && task->remainingPeers.empty()) {
        unified_event::logWarning("BitTorrent.MetadataExchange", "Failed to connect to any peers for info hash: " + types::infoHashToString(task->infoHash));

        // Call the callback
        if (task->callback) {
            task->callback(false);
        }

        // Remove the task
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        m_tasks.erase(types::infoHashToString(task->infoHash));
    }
}

void MetadataExchange::handleSuccessfulAcquisition(std::shared_ptr<PeerConnection> connection) {
    // Find the task for this connection
    std::string taskId = types::infoHashToString(connection->infoHash);
    std::shared_ptr<AcquisitionTask> task;

    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        auto it = m_tasks.find(taskId);
        if (it != m_tasks.end()) {
            task = it->second;
        }
    }

    if (!task) {
        return;
    }

    // Mark the task as completed
    task->completed = true;

    // Call the callback
    if (task->callback) {
        task->callback(true);
    }

    // Disconnect all connections for this task
    std::vector<std::string> connectionsToRemove;
    {
        std::lock_guard<std::mutex> lock(m_connectionsMutex);
        for (auto& [id, conn] : m_connections) {
            if (conn->infoHash == connection->infoHash && conn.get() != connection.get()) {
                conn->client.disconnect();
                connectionsToRemove.push_back(id);
            }
        }

        // Remove the connections
        for (const auto& id : connectionsToRemove) {
            m_connections.erase(id);
        }
    }

    // Remove the task
    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        m_tasks.erase(taskId);
    }
}

void MetadataExchange::handleFailedAcquisition(std::shared_ptr<PeerConnection> connection) {
    // Find the task for this connection
    std::string taskId = types::infoHashToString(connection->infoHash);
    std::shared_ptr<AcquisitionTask> task;

    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        auto it = m_tasks.find(taskId);
        if (it != m_tasks.end()) {
            task = it->second;
        }
    }

    if (!task) {
        return;
    }

    // Decrement the active connections count
    task->activeConnections--;

    // Try to connect to more peers
    tryMorePeers(task);
}

bool MetadataExchange::connectToPeer(const network::EndPoint& peer, std::shared_ptr<PeerConnection> connection) {
    unified_event::logInfo("BitTorrent.MetadataExchange", "Connecting to peer: " + peer.toString() + " for info hash: " + types::infoHashToString(connection->infoHash));

    // Set up the data received callback
    connection->client.setDataReceivedCallback([this, connection](const uint8_t* data, size_t length) {
        handleDataReceived(connection, data, length);
    });

    // Set up the error callback
    connection->client.setErrorCallback([this, connection](const std::string& error) {
        handleConnectionError(connection, error);
    });

    // Set up the connection closed callback
    connection->client.setConnectionClosedCallback([this, connection]() {
        handleConnectionClosed(connection);
    });

    // Connect to the peer
    if (!connection->client.connect(peer.getAddress().toString(), peer.getPort())) {
        unified_event::logWarning("BitTorrent.MetadataExchange", "Failed to connect to peer: " + peer.toString());
        return false;
    }

    // Send the handshake
    if (!sendHandshake(connection)) {
        unified_event::logWarning("BitTorrent.MetadataExchange", "Failed to send handshake to peer: " + peer.toString());
        connection->client.disconnect();
        return false;
    }

    connection->connected = true;
    return true;
}

bool MetadataExchange::sendHandshake(std::shared_ptr<PeerConnection> connection) {
    // BitTorrent handshake format:
    // <pstrlen><pstr><reserved><info_hash><peer_id>
    // pstrlen = 19 (1 byte)
    // pstr = "BitTorrent protocol" (19 bytes)
    // reserved = 8 bytes, with bit 20 (0-indexed) set to indicate support for extension protocol
    // info_hash = 20 bytes
    // peer_id = 20 bytes

    std::vector<uint8_t> handshake(68);

    // pstrlen
    handshake[0] = 19;

    // pstr
    const char* protocol = "BitTorrent protocol";
    std::memcpy(handshake.data() + 1, protocol, 19);

    // reserved (set bit 20 for extension protocol support)
    handshake[25] = 0x10; // 00010000 in binary

    // info_hash
    std::memcpy(handshake.data() + 28, connection->infoHash.data(), 20);

    // peer_id (generate a random peer ID)
    std::string peerId = "-DH0001-";
    for (int i = 0; i < 12; i++) {
        peerId += static_cast<char>('0' + (rand() % 10));
    }
    std::memcpy(handshake.data() + 48, peerId.c_str(), 20);

    // Send the handshake
    return connection->client.send(handshake.data(), handshake.size());
}

void MetadataExchange::handleDataReceived(std::shared_ptr<PeerConnection> connection, const uint8_t* data, size_t length) {
    // Add the data to the buffer
    connection->buffer.insert(connection->buffer.end(), data, data + length);

    // Process the buffer
    if (!connection->handshakeComplete) {
        // Wait for the complete handshake (68 bytes)
        if (connection->buffer.size() >= 68) {
            if (processHandshakeResponse(connection, connection->buffer.data(), 68)) {
                // Remove the handshake from the buffer
                connection->buffer.erase(connection->buffer.begin(), connection->buffer.begin() + 68);

                // Send the extension handshake
                sendExtensionHandshake(connection);
            } else {
                // Handshake failed, disconnect
                connection->client.disconnect();
            }
        }
    } else {
        // Process BitTorrent messages
        while (connection->buffer.size() >= 4) {
            // Get the message length
            uint32_t messageLength = 0;
            messageLength |= static_cast<uint32_t>(connection->buffer[0]) << 24;
            messageLength |= static_cast<uint32_t>(connection->buffer[1]) << 16;
            messageLength |= static_cast<uint32_t>(connection->buffer[2]) << 8;
            messageLength |= static_cast<uint32_t>(connection->buffer[3]);

            // Check if we have the complete message
            if (connection->buffer.size() >= 4 + messageLength) {
                // Process the message
                if (messageLength > 0) {
                    uint8_t messageId = connection->buffer[4];

                    if (messageId == BT_EXTENSION_MESSAGE_ID) {
                        // Extension message
                        if (messageLength >= 2) {
                            uint8_t extensionId = connection->buffer[5];

                            if (extensionId == BT_EXTENSION_HANDSHAKE_ID) {
                                // Extension handshake
                                processExtensionHandshake(connection, connection->buffer.data() + 6, messageLength - 2);
                            } else {
                                // Extension message (ut_metadata)
                                processMetadataResponse(connection, connection->buffer.data() + 5, messageLength - 1);
                            }
                        }
                    }
                }

                // Remove the message from the buffer
                connection->buffer.erase(connection->buffer.begin(), connection->buffer.begin() + 4 + messageLength);
            } else {
                // Wait for more data
                break;
            }
        }
    }
}

bool MetadataExchange::processHandshakeResponse(std::shared_ptr<PeerConnection> connection, const uint8_t* data, size_t length) {
    if (length < 68) {
        unified_event::logWarning("BitTorrent.MetadataExchange", "Handshake response too short");
        return false;
    }

    // Check the protocol string
    if (data[0] != 19 || std::memcmp(data + 1, "BitTorrent protocol", 19) != 0) {
        unified_event::logWarning("BitTorrent.MetadataExchange", "Invalid protocol string in handshake response");
        return false;
    }

    // Check if the peer supports the extension protocol
    if ((data[25] & 0x10) == 0) {
        unified_event::logWarning("BitTorrent.MetadataExchange", "Peer does not support the extension protocol");
        return false;
    }

    // Check the info hash
    if (std::memcmp(data + 28, connection->infoHash.data(), 20) != 0) {
        unified_event::logWarning("BitTorrent.MetadataExchange", "Info hash mismatch in handshake response");
        return false;
    }

    unified_event::logInfo("BitTorrent.MetadataExchange", "Handshake successful for info hash: " + types::infoHashToString(connection->infoHash));
    connection->handshakeComplete = true;
    return true;
}

bool MetadataExchange::sendExtensionHandshake(std::shared_ptr<PeerConnection> connection) {
    // Create the extension handshake dictionary
    auto handshake = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::Dictionary());

    // Add the m dictionary (extension message IDs)
    auto m = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::Dictionary());
    m->setInteger("ut_metadata", 1); // Use message ID 1 for ut_metadata
    handshake->set("m", m);

    // Add metadata_size if we know it
    if (connection->metadataSize > 0) {
        handshake->setInteger("metadata_size", static_cast<int64_t>(connection->metadataSize));
    }

    // Encode the handshake
    std::string encodedHandshake = bencode::BencodeEncoder::encode(handshake);

    // Create the message
    std::vector<uint8_t> message(encodedHandshake.size() + 6);

    // Message length (4 bytes)
    uint32_t messageLength = static_cast<uint32_t>(encodedHandshake.size() + 2);
    message[0] = static_cast<uint8_t>((messageLength >> 24) & 0xFF);
    message[1] = static_cast<uint8_t>((messageLength >> 16) & 0xFF);
    message[2] = static_cast<uint8_t>((messageLength >> 8) & 0xFF);
    message[3] = static_cast<uint8_t>(messageLength & 0xFF);

    // Message ID (1 byte)
    message[4] = BT_EXTENSION_MESSAGE_ID;

    // Extension message ID (1 byte)
    message[5] = BT_EXTENSION_HANDSHAKE_ID;

    // Extension handshake payload
    std::memcpy(message.data() + 6, encodedHandshake.data(), encodedHandshake.size());

    // Send the message
    return connection->client.send(message.data(), message.size());
}

bool MetadataExchange::processExtensionHandshake(std::shared_ptr<PeerConnection> connection, const uint8_t* data, size_t length) {
    try {
        // Decode the handshake
        std::string encodedHandshake(reinterpret_cast<const char*>(data), length);
        auto handshake = bencode::BencodeDecoder::decode(encodedHandshake);

        if (!handshake || !handshake->isDictionary()) {
            unified_event::logWarning("BitTorrent.MetadataExchange", "Invalid extension handshake");
            return false;
        }

        // Get the m dictionary
        auto m = handshake->getDictionary("m");
        if (!m) {
            unified_event::logWarning("BitTorrent.MetadataExchange", "Missing or invalid m dictionary in extension handshake");
            return false;
        }

        // Create a BencodeValue from the dictionary to access its methods
        auto mValue = std::make_shared<bencode::BencodeValue>(*m);

        // Get the ut_metadata message ID
        auto utMetadata = mValue->getInteger("ut_metadata");
        if (!utMetadata) {
            unified_event::logWarning("BitTorrent.MetadataExchange", "Missing or invalid ut_metadata in extension handshake");
            return false;
        }

        // Get the metadata size
        auto metadataSize = handshake->getInteger("metadata_size");
        if (!metadataSize) {
            unified_event::logWarning("BitTorrent.MetadataExchange", "Missing or invalid metadata_size in extension handshake");
            return false;
        }

        // Store the metadata size
        connection->metadataSize = static_cast<int>(*metadataSize);

        unified_event::logInfo("BitTorrent.MetadataExchange", "Extension handshake successful for info hash: " +
                               types::infoHashToString(connection->infoHash) +
                               ", metadata size: " + std::to_string(connection->metadataSize));

        connection->extensionHandshakeComplete = true;

        // Request all metadata pieces
        int pieceCount = (connection->metadataSize + 16383) / 16384; // 16 KB pieces
        for (int piece = 0; piece < pieceCount; piece++) {
            sendMetadataRequest(connection, piece);
        }

        return true;
    } catch (const std::exception& e) {
        unified_event::logError("BitTorrent.MetadataExchange", "Error processing extension handshake: " + std::string(e.what()));
        return false;
    }
}

bool MetadataExchange::sendMetadataRequest(std::shared_ptr<PeerConnection> connection, int piece) {
    // Create the metadata request dictionary
    auto request = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::Dictionary());

    // Add the msg_type (0 = request)
    request->setInteger("msg_type", 0);

    // Add the piece
    request->setInteger("piece", static_cast<int64_t>(piece));

    // Encode the request
    std::string encodedRequest = bencode::BencodeEncoder::encode(request);

    // Create the message
    std::vector<uint8_t> message(encodedRequest.size() + 6);

    // Message length (4 bytes)
    uint32_t messageLength = static_cast<uint32_t>(encodedRequest.size() + 2);
    message[0] = static_cast<uint8_t>((messageLength >> 24) & 0xFF);
    message[1] = static_cast<uint8_t>((messageLength >> 16) & 0xFF);
    message[2] = static_cast<uint8_t>((messageLength >> 8) & 0xFF);
    message[3] = static_cast<uint8_t>(messageLength & 0xFF);

    // Message ID (1 byte)
    message[4] = BT_EXTENSION_MESSAGE_ID;

    // Extension message ID (1 byte) - use 1 for ut_metadata
    message[5] = 1;

    // Extension message payload
    std::memcpy(message.data() + 6, encodedRequest.data(), encodedRequest.size());

    // Send the message
    return connection->client.send(message.data(), message.size());
}

bool MetadataExchange::processMetadataResponse(std::shared_ptr<PeerConnection> connection, const uint8_t* data, size_t length) {
    try {
        // The first byte is the extension message ID
        // The first byte is the extension message ID (unused)

        // The rest is the bencoded data
        std::string encodedData(reinterpret_cast<const char*>(data + 1), length - 1);
        auto response = bencode::BencodeDecoder::decode(encodedData);

        if (!response || !response->isDictionary()) {
            unified_event::logWarning("BitTorrent.MetadataExchange", "Invalid metadata response");
            return false;
        }

        // Get the msg_type
        auto msgType = response->getInteger("msg_type");
        if (!msgType) {
            unified_event::logWarning("BitTorrent.MetadataExchange", "Missing or invalid msg_type in metadata response");
            return false;
        }

        int type = static_cast<int>(*msgType);

        if (type == 1) {
            // Data message

            // Get the piece
            auto piece = response->getInteger("piece");
            if (!piece) {
                unified_event::logWarning("BitTorrent.MetadataExchange", "Missing or invalid piece in metadata response");
                return false;
            }

            int pieceIndex = static_cast<int>(*piece);

            // Find the total message size
            size_t totalSize = encodedData.size();
            size_t headerSize = 0;

            // Find the end of the bencoded dictionary
            for (size_t i = 0; i < encodedData.size(); i++) {
                if (encodedData[i] == 'e') {
                    headerSize = i + 1;
                    break;
                }
            }

            if (headerSize == 0 || headerSize >= totalSize) {
                unified_event::logWarning("BitTorrent.MetadataExchange", "Invalid metadata response format");
                return false;
            }

            // Extract the metadata piece
            std::vector<uint8_t> pieceData(encodedData.begin() + static_cast<std::ptrdiff_t>(headerSize), encodedData.end());

            // Store the piece
            std::lock_guard<std::mutex> lock(connection->mutex);
            connection->metadataPieces[pieceIndex] = pieceData;

            unified_event::logInfo("BitTorrent.MetadataExchange", "Received metadata piece " +
                                   std::to_string(pieceIndex) +
                                   " for info hash: " +
                                   types::infoHashToString(connection->infoHash) +
                                   " (" +
                                   std::to_string(connection->metadataPieces.size()) +
                                   "/" +
                                   std::to_string((connection->metadataSize + 16383) / 16384) +
                                   ")");

            // Check if we have all pieces
            int pieceCount = (connection->metadataSize + 16383) / 16384;
            if (static_cast<int>(connection->metadataPieces.size()) == pieceCount) {
                // Process the complete metadata
                processCompleteMetadata(connection);
            }

            return true;
        } else if (type == 2) {
            // Reject message
            unified_event::logWarning("BitTorrent.MetadataExchange", "Metadata request rejected by peer");
            return false;
        }

        return false;
    } catch (const std::exception& e) {
        unified_event::logError("BitTorrent.MetadataExchange", "Error processing metadata response: " + std::string(e.what()));
        return false;
    }
}

bool MetadataExchange::processCompleteMetadata(std::shared_ptr<PeerConnection> connection) {
    connection->metadataExchangeComplete = true;
    try {
        // Combine all pieces
        std::vector<uint8_t> completeMetadata;
        completeMetadata.reserve(static_cast<size_t>(connection->metadataSize));

        int pieceCount = (connection->metadataSize + 16383) / 16384;
        for (int i = 0; i < pieceCount; i++) {
            auto it = connection->metadataPieces.find(i);
            if (it == connection->metadataPieces.end()) {
                unified_event::logWarning("BitTorrent.MetadataExchange", "Missing metadata piece " + std::to_string(i));
                return false;
            }

            completeMetadata.insert(completeMetadata.end(), it->second.begin(), it->second.end());
        }

        // Truncate to the correct size
        if (completeMetadata.size() > static_cast<size_t>(connection->metadataSize)) {
            completeMetadata.resize(static_cast<size_t>(connection->metadataSize));
        }

        // Verify the info hash
        std::array<uint8_t, 20> hash = utility::hash::sha1(completeMetadata);
        if (std::memcmp(hash.data(), connection->infoHash.data(), 20) != 0) {
            unified_event::logWarning("BitTorrent.MetadataExchange", "Metadata hash mismatch");
            return false;
        }

        // Decode the metadata
        std::string encodedMetadata(reinterpret_cast<const char*>(completeMetadata.data()), completeMetadata.size());
        auto metadata = bencode::BencodeDecoder::decode(encodedMetadata);

        if (!metadata || !metadata->isDictionary()) {
            unified_event::logWarning("BitTorrent.MetadataExchange", "Invalid metadata format");
            return false;
        }

        // Extract the name
        auto name = metadata->getString("name");
        if (!name) {
            unified_event::logWarning("BitTorrent.MetadataExchange", "Missing or invalid name in metadata");
            return false;
        }

        std::string torrentName = *name;

        // Extract the files
        std::vector<types::TorrentFile> files;
        uint64_t totalSize = 0;

        auto filesList = metadata->getList("files");
        if (filesList) {
            // Multi-file torrent
            for (const auto& fileEntry : *filesList) {
                if (!fileEntry->isDictionary()) {
                    continue;
                }

                auto length = fileEntry->getInteger("length");
                auto pathList = fileEntry->getList("path");

                if (!length || !pathList) {
                    continue;
                }

                uint64_t fileSize = static_cast<uint64_t>(*length);

                // Combine path elements
                std::string filePath;
                for (size_t i = 0; i < pathList->size(); i++) {
                    auto pathElementValue = (*pathList)[i];
                    if (!pathElementValue || !pathElementValue->isString()) {
                        continue;
                    }

                    std::string pathElement = pathElementValue->getString();

                    if (i > 0) {
                        filePath += "/";
                    }

                    filePath += pathElement;
                }

                if (!filePath.empty()) {
                    files.emplace_back(filePath, fileSize);
                    totalSize += fileSize;
                }
            }
        } else {
            // Single-file torrent
            auto length = metadata->getInteger("length");
            if (length) {
                uint64_t fileSize = static_cast<uint64_t>(*length);
                files.emplace_back(torrentName, fileSize);
                totalSize = fileSize;
            }
        }

        // Store the metadata
        utility::metadata::MetadataUtils::setInfoHashName(connection->infoHash, torrentName);

        for (const auto& file : files) {
            utility::metadata::MetadataUtils::addFileToInfoHash(connection->infoHash, file.path, file.size);
        }

        unified_event::logInfo("BitTorrent.MetadataExchange", "Successfully acquired metadata for info hash: " +
                              types::infoHashToString(connection->infoHash) +
                              ", name: " +
                              torrentName +
                              ", size: " +
                              std::to_string(totalSize) +
                              " bytes, files: " +
                              std::to_string(files.size()));

        // Handle the successful acquisition
        handleSuccessfulAcquisition(connection);

        // Disconnect from the peer
        connection->client.disconnect();

        return true;
    } catch (const std::exception& e) {
        unified_event::logError("BitTorrent.MetadataExchange", "Error processing complete metadata: " + std::string(e.what()));
        return false;
    }
}

void MetadataExchange::handleConnectionError(std::shared_ptr<PeerConnection> connection, const std::string& error) {
    unified_event::logWarning("BitTorrent.MetadataExchange", "Connection error for info hash: " +
                             types::infoHashToString(connection->infoHash) +
                             ", error: " +
                             error);

    connection->failed = true;

    // Handle the failed acquisition
    handleFailedAcquisition(connection);
}

void MetadataExchange::handleConnectionClosed(std::shared_ptr<PeerConnection> connection) {
    unified_event::logDebug("BitTorrent.MetadataExchange", "Connection closed for info hash: " +
                           types::infoHashToString(connection->infoHash));

    // If the metadata exchange was completed successfully, handle it
    if (connection->metadataExchangeComplete) {
        handleSuccessfulAcquisition(connection);
    } else {
        // Otherwise, handle the failed acquisition
        handleFailedAcquisition(connection);
    }
}

void MetadataExchange::cleanupTimedOutConnections() {
    // Create a list of connections to disconnect
    std::vector<std::shared_ptr<PeerConnection>> connectionsToDisconnect;
    std::vector<std::string> connectionsToRemove;

    {
        std::lock_guard<std::mutex> lock(m_connectionsMutex);

        auto now = std::chrono::steady_clock::now();

        for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
            auto& connection = it->second;

            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - connection->startTime).count();

            if (elapsed > CONNECTION_TIMEOUT_SECONDS || connection->failed || connection->metadataExchangeComplete) {
                // Add to the list of connections to disconnect
                connectionsToDisconnect.push_back(connection);
                connectionsToRemove.push_back(it->first);
            }
        }

        // Remove the connections from the map
        for (const auto& id : connectionsToRemove) {
            m_connections.erase(id);
        }
    }

    // Disconnect the connections outside the lock
    for (auto& connection : connectionsToDisconnect) {
        // Disconnect the client
        connection->client.disconnect();

        // Handle the failed acquisition
        handleFailedAcquisition(connection);
    }
}

void MetadataExchange::cleanupTimedOutConnectionsPeriodically() {
    while (m_running) {
        // Wait for the cleanup interval or until we're stopped
        std::unique_lock<std::mutex> lock(m_cleanupMutex);
        m_cleanupCondition.wait_for(lock, std::chrono::seconds(CLEANUP_INTERVAL_SECONDS), [this]() {
            return !m_running;
        });

        if (!m_running) {
            break;
        }

        // Clean up timed out connections
        cleanupTimedOutConnections();
    }
}

} // namespace dht_hunter::bittorrent::metadata
