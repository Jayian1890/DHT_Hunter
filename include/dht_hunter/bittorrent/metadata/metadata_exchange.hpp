#pragma once

#include "dht_hunter/types/info_hash.hpp"
#include "dht_hunter/types/endpoint.hpp"
#include "dht_hunter/network/tcp_client.hpp"
#include "dht_hunter/bencode/bencode.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

namespace dht_hunter::bittorrent::metadata {

/**
 * @brief Implements the BitTorrent Metadata Exchange Protocol (BEP 9)
 * 
 * This class is responsible for acquiring metadata for InfoHashes using
 * the BitTorrent Extension Protocol and the ut_metadata extension.
 */
class MetadataExchange {
public:
    /**
     * @brief Constructor
     */
    MetadataExchange();

    /**
     * @brief Destructor
     */
    ~MetadataExchange();

    /**
     * @brief Acquires metadata for an InfoHash
     * @param infoHash The InfoHash to acquire metadata for
     * @param peers The peers to connect to
     * @param callback The callback to call when metadata is acquired
     * @return True if the acquisition was started, false otherwise
     */
    bool acquireMetadata(
        const types::InfoHash& infoHash,
        const std::vector<network::EndPoint>& peers,
        std::function<void(bool success)> callback);

private:
    /**
     * @brief Represents a peer connection for metadata exchange
     */
    struct PeerConnection {
        network::TCPClient client;
        types::InfoHash infoHash;
        std::vector<uint8_t> buffer;
        std::atomic<bool> handshakeComplete{false};
        std::atomic<bool> extensionHandshakeComplete{false};
        std::atomic<bool> metadataExchangeComplete{false};
        std::atomic<bool> connected{false};
        std::atomic<bool> failed{false};
        std::atomic<int> metadataSize{0};
        std::unordered_map<int, std::vector<uint8_t>> metadataPieces;
        std::function<void(bool success)> callback;
        std::chrono::steady_clock::time_point startTime;
        std::mutex mutex;

        PeerConnection(const types::InfoHash& ih, std::function<void(bool success)> cb)
            : infoHash(ih), callback(cb), startTime(std::chrono::steady_clock::now()) {}
    };

    /**
     * @brief Connects to a peer
     * @param peer The peer to connect to
     * @param connection The peer connection
     * @return True if the connection was established, false otherwise
     */
    bool connectToPeer(const network::EndPoint& peer, std::shared_ptr<PeerConnection> connection);

    /**
     * @brief Sends the BitTorrent handshake
     * @param connection The peer connection
     * @return True if the handshake was sent, false otherwise
     */
    bool sendHandshake(std::shared_ptr<PeerConnection> connection);

    /**
     * @brief Processes the BitTorrent handshake response
     * @param connection The peer connection
     * @param data The handshake response data
     * @param length The handshake response length
     * @return True if the handshake was processed successfully, false otherwise
     */
    bool processHandshakeResponse(std::shared_ptr<PeerConnection> connection, const uint8_t* data, size_t length);

    /**
     * @brief Sends the extension handshake
     * @param connection The peer connection
     * @return True if the handshake was sent, false otherwise
     */
    bool sendExtensionHandshake(std::shared_ptr<PeerConnection> connection);

    /**
     * @brief Processes the extension handshake response
     * @param connection The peer connection
     * @param data The handshake response data
     * @param length The handshake response length
     * @return True if the handshake was processed successfully, false otherwise
     */
    bool processExtensionHandshake(std::shared_ptr<PeerConnection> connection, const uint8_t* data, size_t length);

    /**
     * @brief Sends a metadata request
     * @param connection The peer connection
     * @param piece The piece to request
     * @return True if the request was sent, false otherwise
     */
    bool sendMetadataRequest(std::shared_ptr<PeerConnection> connection, int piece);

    /**
     * @brief Processes a metadata response
     * @param connection The peer connection
     * @param data The response data
     * @param length The response length
     * @return True if the response was processed successfully, false otherwise
     */
    bool processMetadataResponse(std::shared_ptr<PeerConnection> connection, const uint8_t* data, size_t length);

    /**
     * @brief Processes a complete metadata
     * @param connection The peer connection
     * @return True if the metadata was processed successfully, false otherwise
     */
    bool processCompleteMetadata(std::shared_ptr<PeerConnection> connection);

    /**
     * @brief Handles data received from a peer
     * @param connection The peer connection
     * @param data The received data
     * @param length The received data length
     */
    void handleDataReceived(std::shared_ptr<PeerConnection> connection, const uint8_t* data, size_t length);

    /**
     * @brief Handles a connection error
     * @param connection The peer connection
     * @param error The error message
     */
    void handleConnectionError(std::shared_ptr<PeerConnection> connection, const std::string& error);

    /**
     * @brief Handles a connection closed
     * @param connection The peer connection
     */
    void handleConnectionClosed(std::shared_ptr<PeerConnection> connection);

    /**
     * @brief Cleans up timed out connections
     */
    void cleanupTimedOutConnections();

    /**
     * @brief Cleans up timed out connections periodically
     */
    void cleanupTimedOutConnectionsPeriodically();

    // Active connections
    std::unordered_map<std::string, std::shared_ptr<PeerConnection>> m_connections;
    std::mutex m_connectionsMutex;

    // Cleanup thread
    std::thread m_cleanupThread;
    std::atomic<bool> m_running{true};
    std::condition_variable m_cleanupCondition;
    std::mutex m_cleanupMutex;

    // Extension message IDs
    static constexpr uint8_t BT_EXTENSION_MESSAGE_ID = 20;
    static constexpr uint8_t BT_EXTENSION_HANDSHAKE_ID = 0;

    // Timeout values
    static constexpr int CONNECTION_TIMEOUT_SECONDS = 30;
    static constexpr int CLEANUP_INTERVAL_SECONDS = 5;
};

} // namespace dht_hunter::bittorrent::metadata
