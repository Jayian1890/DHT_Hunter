#pragma once

#include "dht_hunter/types/info_hash.hpp"
#include "dht_hunter/dht/storage/peer_storage.hpp"
#include "dht_hunter/bittorrent/metadata/metadata_exchange.hpp"
#include "dht_hunter/bittorrent/metadata/peer_health_tracker.hpp"
#include "dht_hunter/bittorrent/metadata/dht_metadata_provider.hpp"
#include "dht_hunter/bittorrent/tracker/tracker_manager.hpp"
#include "dht_hunter/network/nat/nat_traversal_manager.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include "dht_hunter/utility/config/configuration_manager.hpp"

#include <memory>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <condition_variable>

namespace dht_hunter::bittorrent::metadata {

/**
 * @brief Manages the acquisition of metadata for InfoHashes
 *
 * This class is responsible for coordinating the acquisition of metadata
 * for InfoHashes using the BitTorrent Metadata Exchange Protocol.
 */
class MetadataAcquisitionManager {
public:
    /**
     * @brief Gets the singleton instance
     * @param peerStorage The peer storage
     * @return The singleton instance
     */
    static std::shared_ptr<MetadataAcquisitionManager> getInstance(
        std::shared_ptr<dht::PeerStorage> peerStorage = nullptr);

    /**
     * @brief Destructor
     */
    ~MetadataAcquisitionManager();

    /**
     * @brief Starts the metadata acquisition manager
     * @return True if the manager was started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stops the metadata acquisition manager
     */
    void stop();

    /**
     * @brief Checks if the metadata acquisition manager is running
     * @return True if the manager is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Acquires metadata for an InfoHash
     * @param infoHash The InfoHash to acquire metadata for
     * @param priority Priority of the acquisition (higher values = higher priority)
     * @return True if the acquisition was started, false otherwise
     */
    bool acquireMetadata(const types::InfoHash& infoHash, int priority = 0);

    /**
     * @brief Gets the acquisition status for an InfoHash
     * @param infoHash The InfoHash to check
     * @return A string describing the acquisition status
     */
    std::string getAcquisitionStatus(const types::InfoHash& infoHash) const;

    /**
     * @brief Gets the acquisition statistics
     * @return A map of statistics
     */
    std::unordered_map<std::string, int> getStatistics() const;

    /**
     * @brief Checks if metadata acquisition is in progress for an InfoHash
     * @param infoHash The InfoHash to check
     * @return True if acquisition is in progress, false otherwise
     */
    bool isAcquisitionInProgress(const types::InfoHash& infoHash) const;

    /**
     * @brief Gets the number of active acquisitions
     * @return The number of active acquisitions
     */
    size_t getActiveAcquisitionCount() const;

private:
    /**
     * @brief Private constructor for singleton pattern
     * @param peerStorage The peer storage
     */
    explicit MetadataAcquisitionManager(std::shared_ptr<dht::PeerStorage> peerStorage);

    /**
     * @brief Prioritizes peers for metadata acquisition
     * @param peers The list of peers to prioritize
     * @return The prioritized list of peers
     */
    std::vector<network::EndPoint> prioritizePeers(const std::vector<network::EndPoint>& peers);

    /**
     * @brief Processes the acquisition queue
     */
    void processAcquisitionQueue();

    /**
     * @brief Processes the acquisition queue periodically
     */
    void processAcquisitionQueuePeriodically();

    /**
     * @brief Handles a new InfoHash event
     * @param event The event
     */
    void handleInfoHashDiscoveredEvent(const std::shared_ptr<unified_event::InfoHashDiscoveredEvent>& event);

    // Static instance for singleton pattern
    static std::shared_ptr<MetadataAcquisitionManager> s_instance;
    static std::mutex s_instanceMutex;

    // Dependencies
    std::shared_ptr<dht::PeerStorage> m_peerStorage;
    std::shared_ptr<MetadataExchange> m_metadataExchange;
    std::shared_ptr<DHTMetadataProvider> m_dhtMetadataProvider;
    std::shared_ptr<tracker::TrackerManager> m_trackerManager;
    std::shared_ptr<network::nat::NATTraversalManager> m_natTraversalManager;
    std::shared_ptr<unified_event::EventBus> m_eventBus;

    // Acquisition queue
    std::unordered_set<types::InfoHash> m_acquisitionQueue;
    mutable std::mutex m_queueMutex;

    /**
     * @brief Represents an acquisition task
     */
    struct AcquisitionTask {
        types::InfoHash infoHash;
        int priority;
        std::string status;
        int retryCount;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastRetryTime;

        AcquisitionTask(const types::InfoHash& ih, int p)
            : infoHash(ih), priority(p), status("Queued"), retryCount(0),
              startTime(std::chrono::steady_clock::now()),
              lastRetryTime(std::chrono::steady_clock::now()) {}
    };

    // Active acquisitions
    std::unordered_map<types::InfoHash, std::shared_ptr<AcquisitionTask>> m_activeAcquisitions;
    mutable std::mutex m_activeAcquisitionsMutex;

    // Failed acquisitions for retry
    std::unordered_map<types::InfoHash, std::shared_ptr<AcquisitionTask>> m_failedAcquisitions;
    mutable std::mutex m_failedAcquisitionsMutex;

    // Acquisition statistics
    std::atomic<int> m_totalAttempts{0};
    std::atomic<int> m_successfulAcquisitionsCount{0};
    std::atomic<int> m_failedAcquisitionsCount{0};
    std::atomic<int> m_retryAttempts{0};

    // Processing thread
    std::thread m_processingThread;
    std::atomic<bool> m_running{false};
    std::condition_variable m_processingCondition;
    std::mutex m_processingMutex;

    // Event subscription
    int m_infoHashDiscoveredSubscription = 0;

    /**
     * @brief Processes failed acquisitions for retry
     */
    void processFailedAcquisitions();

    /**
     * @brief Handles a successful metadata acquisition
     * @param infoHash The InfoHash that was acquired
     */
    void handleSuccessfulAcquisition(const types::InfoHash& infoHash);

    /**
     * @brief Handles a failed metadata acquisition
     * @param infoHash The InfoHash that failed
     * @param error The error message
     */
    void handleFailedAcquisition(const types::InfoHash& infoHash, const std::string& error);

    // Configuration parameters
    int m_processingIntervalSeconds;
    int m_maxConcurrentAcquisitions;
    int m_acquisitionTimeoutSeconds;
    int m_maxRetryCount;
    int m_retryDelayBaseSeconds;

    // Load configuration from the configuration manager
    void loadConfiguration();
};

} // namespace dht_hunter::bittorrent::metadata
