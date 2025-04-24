#pragma once

#include "dht_hunter/dht/types.hpp"
#include "dht_hunter/logforge/logforge.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace dht_hunter::crawler {

/**
 * @brief Configuration for the InfoHashCollector
 */
struct InfoHashCollectorConfig {
    uint32_t maxStoredInfoHashes = 1000000;  ///< Maximum number of infohashes to store
    uint32_t maxQueueSize = 10000;           ///< Maximum size of the processing queue
    bool deduplicateInfoHashes = true;       ///< Whether to deduplicate infohashes
    bool filterInvalidInfoHashes = true;     ///< Whether to filter out invalid infohashes (all zeros, etc.)
    std::chrono::seconds saveInterval = std::chrono::seconds(300); ///< Interval for saving infohashes to disk
    std::string savePath = "config/infohashes.dat"; ///< Path for saving infohashes to disk
};

/**
 * @brief Callback for new infohash notifications
 * @param infoHash The infohash
 */
using InfoHashCallback = std::function<void(const dht_hunter::dht::InfoHash&)>;

/**
 * @brief Class for collecting infohashes from the DHT network
 */
class InfoHashCollector {
public:
    /**
     * @brief Constructor
     * @param config The configuration
     */
    explicit InfoHashCollector(const InfoHashCollectorConfig& config = InfoHashCollectorConfig());

    /**
     * @brief Destructor
     */
    ~InfoHashCollector();

    /**
     * @brief Starts the collector
     * @return True if started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stops the collector
     */
    void stop();

    /**
     * @brief Checks if the collector is running
     * @return True if running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Adds an infohash to the collection
     * @param infoHash The infohash to add
     * @return True if the infohash was added, false otherwise
     */
    bool addInfoHash(const dht_hunter::dht::InfoHash& infoHash);

    /**
     * @brief Gets the number of collected infohashes
     * @return The number of collected infohashes
     */
    size_t getInfoHashCount() const;

    /**
     * @brief Gets a copy of all collected infohashes
     * @return A vector of all collected infohashes
     */
    std::vector<dht_hunter::dht::InfoHash> getInfoHashes() const;

    /**
     * @brief Gets a batch of collected infohashes
     * @param count The maximum number of infohashes to get
     * @return A vector of collected infohashes
     */
    std::vector<dht_hunter::dht::InfoHash> getInfoHashBatch(size_t count) const;

    /**
     * @brief Clears all collected infohashes
     */
    void clearInfoHashes();

    /**
     * @brief Saves collected infohashes to a file
     * @param filePath The path to save to
     * @return True if saved successfully, false otherwise
     */
    bool saveInfoHashes(const std::string& filePath) const;

    /**
     * @brief Loads infohashes from a file
     * @param filePath The path to load from
     * @return True if loaded successfully, false otherwise
     */
    bool loadInfoHashes(const std::string& filePath);

    /**
     * @brief Sets the callback for new infohash notifications
     * @param callback The callback function
     */
    void setNewInfoHashCallback(InfoHashCallback callback);

    /**
     * @brief Gets the current configuration
     * @return The current configuration
     */
    InfoHashCollectorConfig getConfig() const;

    /**
     * @brief Sets the configuration
     * @param config The new configuration
     */
    void setConfig(const InfoHashCollectorConfig& config);

private:
    /**
     * @brief Processes the infohash queue
     */
    void processQueue();

    /**
     * @brief Saves infohashes periodically
     */
    void periodicSave();

    /**
     * @brief Checks if an infohash is valid
     * @param infoHash The infohash to check
     * @return True if valid, false otherwise
     */
    bool isValidInfoHash(const dht_hunter::dht::InfoHash& infoHash) const;

    InfoHashCollectorConfig m_config;                  ///< Configuration
    std::atomic<bool> m_running{false};                ///< Whether the collector is running
    mutable std::mutex m_mutex;                        ///< Mutex for thread safety
    std::unordered_set<std::string> m_infoHashes;      ///< Set of collected infohashes (as hex strings for deduplication)
    std::queue<dht_hunter::dht::InfoHash> m_queue;     ///< Queue of infohashes to process
    std::thread m_processingThread;                    ///< Thread for processing the queue
    std::thread m_saveThread;                          ///< Thread for periodic saving
    std::string m_savePath;                            ///< Path for automatic saving
    InfoHashCallback m_newInfoHashCallback;            ///< Callback for new infohash notifications
    std::atomic<size_t> m_totalProcessed{0};           ///< Total number of processed infohashes
    std::atomic<size_t> m_totalAdded{0};               ///< Total number of added infohashes
    std::atomic<size_t> m_totalDuplicates{0};          ///< Total number of duplicate infohashes
    std::atomic<size_t> m_totalInvalid{0};             ///< Total number of invalid infohashes
};

} // namespace dht_hunter::crawler
