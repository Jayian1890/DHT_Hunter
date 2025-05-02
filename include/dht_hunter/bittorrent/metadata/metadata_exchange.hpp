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
        std::string peerId;  // Unique identifier for this peer connection
        network::EndPoint peer; // The peer endpoint
        int retryCount{0};   // Number of retry attempts
        std::chrono::steady_clock::time_point startTime;
        std::mutex mutex;

        PeerConnection(const types::InfoHash& ih, const network::EndPoint& p)
            : infoHash(ih), peer(p), startTime(std::chrono::steady_clock::now()) {
            peerId = p.toString();
        }
    };

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
     * @param maxConcurrentPeers Maximum number of peers to connect to concurrently
     * @return True if the acquisition was started, false otherwise
     */
    bool acquireMetadata(
        const types::InfoHash& infoHash,
        const std::vector<network::EndPoint>& peers,
        std::function<void(bool success)> callback,
        int maxConcurrentPeers = 3);

    /**
     * @brief Cancels metadata acquisition for an InfoHash
     * @param infoHash The InfoHash to cancel acquisition for
     */
    void cancelAcquisition(const types::InfoHash& infoHash);

    /**
     * @brief Retries a failed connection
     * @param connection The connection to retry
     * @return True if the retry was successful, false otherwise
     */
    bool retryConnection(std::shared_ptr<PeerConnection> connection);

private:

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

    /**
     * @brief Represents an acquisition task for an InfoHash
     */
    struct AcquisitionTask {
        types::InfoHash infoHash;
        std::vector<network::EndPoint> peers;
        std::vector<network::EndPoint> remainingPeers;
        std::function<void(bool success)> callback;
        int maxConcurrentPeers;
        std::atomic<int> activeConnections{0};
        std::atomic<bool> completed{false};
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastRetryTime;
        int retryCount{0};

        AcquisitionTask(const types::InfoHash& ih,
                       const std::vector<network::EndPoint>& p,
                       std::function<void(bool success)> cb,
                       int maxPeers)
            : infoHash(ih), peers(p), remainingPeers(p), callback(cb),
              maxConcurrentPeers(maxPeers), startTime(std::chrono::steady_clock::now()),
              lastRetryTime(std::chrono::steady_clock::now()) {}
    };

    // Active connections and tasks
    std::unordered_map<std::string, std::shared_ptr<PeerConnection>> m_connections;
    std::unordered_map<std::string, std::shared_ptr<AcquisitionTask>> m_tasks;
    std::mutex m_connectionsMutex;
    std::mutex m_tasksMutex;

    /**
     * @brief Tries to connect to more peers for a task
     * @param task The acquisition task
     */
    void tryMorePeers(std::shared_ptr<AcquisitionTask> task);

    /**
     * @brief Handles a successful metadata acquisition
     * @param connection The connection that acquired the metadata
     */
    void handleSuccessfulAcquisition(std::shared_ptr<PeerConnection> connection);

    /**
     * @brief Handles a failed metadata acquisition
     * @param connection The connection that failed
     */
    void handleFailedAcquisition(std::shared_ptr<PeerConnection> connection);

    // Cleanup thread
    std::thread m_cleanupThread;
    std::atomic<bool> m_running{true};
    std::condition_variable m_cleanupCondition;
    std::mutex m_cleanupMutex;

    // Extension message IDs
    static constexpr uint8_t BT_EXTENSION_MESSAGE_ID = 20;
    static constexpr uint8_t BT_EXTENSION_HANDSHAKE_ID = 0;

    // Timeout and retry values
    static constexpr int CONNECTION_TIMEOUT_SECONDS = 30;
    static constexpr int CLEANUP_INTERVAL_SECONDS = 5;
    static constexpr int MAX_RETRY_COUNT = 3;
    static constexpr int RETRY_DELAY_BASE_SECONDS = 60; // Base delay for exponential backoff
    static constexpr int MAX_CONCURRENT_PEERS_PER_INFOHASH = 5;
};

} // namespace dht_hunter::bittorrent::metadata
