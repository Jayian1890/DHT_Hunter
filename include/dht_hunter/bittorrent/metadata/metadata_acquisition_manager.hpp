#pragma once

#include "dht_hunter/types/info_hash.hpp"
#include "dht_hunter/dht/storage/peer_storage.hpp"
#include "dht_hunter/bittorrent/metadata/metadata_exchange.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"

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
     * @return True if the acquisition was started, false otherwise
     */
    bool acquireMetadata(const types::InfoHash& infoHash);

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
    std::shared_ptr<unified_event::EventBus> m_eventBus;

    // Acquisition queue
    std::unordered_set<types::InfoHash> m_acquisitionQueue;
    mutable std::mutex m_queueMutex;

    // Active acquisitions
    std::unordered_map<types::InfoHash, std::chrono::steady_clock::time_point> m_activeAcquisitions;
    mutable std::mutex m_activeAcquisitionsMutex;

    // Processing thread
    std::thread m_processingThread;
    std::atomic<bool> m_running{false};
    std::condition_variable m_processingCondition;
    std::mutex m_processingMutex;

    // Event subscription
    int m_infoHashDiscoveredSubscription = 0;

    // Constants
    static constexpr int PROCESSING_INTERVAL_SECONDS = 5;
    static constexpr int MAX_CONCURRENT_ACQUISITIONS = 5;
    static constexpr int ACQUISITION_TIMEOUT_SECONDS = 60;
};

} // namespace dht_hunter::bittorrent::metadata
