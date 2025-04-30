#pragma once

#include "dht_hunter/dht/core/routing_table.hpp"
#include "dht_hunter/dht/storage/peer_storage.hpp"
#include "dht_hunter/types/info_hash_metadata.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

namespace dht_hunter::dht {

/**
 * @brief Manages persistence of DHT data (Singleton)
 */
class PersistenceManager {
public:
    /**
     * @brief Gets the singleton instance of the persistence manager
     * @param configDir The configuration directory
     * @return The singleton instance
     */
    static std::shared_ptr<PersistenceManager> getInstance(const std::string& configDir);

    /**
     * @brief Destructor
     */
    ~PersistenceManager();

    /**
     * @brief Delete copy constructor and assignment operator
     */
    PersistenceManager(const PersistenceManager&) = delete;
    PersistenceManager& operator=(const PersistenceManager&) = delete;
    PersistenceManager(PersistenceManager&&) = delete;
    PersistenceManager& operator=(PersistenceManager&&) = delete;

    /**
     * @brief Starts the persistence manager
     * @param routingTable The routing table to persist
     * @param peerStorage The peer storage to persist
     * @return True if the persistence manager was started successfully, false otherwise
     */
    bool start(std::shared_ptr<RoutingTable> routingTable, std::shared_ptr<PeerStorage> peerStorage);

    /**
     * @brief Stops the persistence manager
     */
    void stop();

    /**
     * @brief Checks if the persistence manager is running
     * @return True if the persistence manager is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Saves data to disk immediately
     * @return True if the data was saved successfully, false otherwise
     */
    bool saveNow();

    /**
     * @brief Loads data from disk
     * @return True if the data was loaded successfully, false otherwise
     */
    bool loadFromDisk();

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    explicit PersistenceManager(const std::string& configDir);

    /**
     * @brief Saves data to disk periodically
     */
    void savePeriodically();

    /**
     * @brief Creates the configuration directory if it doesn't exist
     * @return True if the directory exists or was created successfully, false otherwise
     */
    bool createConfigDir();

    // Static instance for singleton pattern
    static std::shared_ptr<PersistenceManager> s_instance;
    static std::mutex s_instanceMutex;

    std::string m_configDir;
    std::string m_routingTablePath;
    std::string m_peerStoragePath;
    std::string m_metadataPath;
    std::atomic<bool> m_running;
    std::thread m_saveThread;
    mutable std::mutex m_mutex;
    std::shared_ptr<RoutingTable> m_routingTable;
    std::shared_ptr<PeerStorage> m_peerStorage;
    std::shared_ptr<types::InfoHashMetadataRegistry> m_metadataRegistry;
    std::chrono::minutes m_saveInterval;
};

} // namespace dht_hunter::dht
