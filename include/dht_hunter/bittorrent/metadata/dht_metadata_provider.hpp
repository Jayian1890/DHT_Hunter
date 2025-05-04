#pragma once

#include "dht_hunter/types/info_hash.hpp"
#include "dht_hunter/dht/core/dht_node.hpp"
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
#include <set>

namespace dht_hunter::bittorrent::metadata {

/**
 * @brief Implements the DHT Metadata Protocol (BEP 51)
 *
 * This class is responsible for acquiring metadata for InfoHashes using
 * the DHT Metadata Protocol.
 */
class DHTMetadataProvider {
public:
    /**
     * @brief Constructor
     * @param dhtNode The DHT node
     */
    explicit DHTMetadataProvider(std::shared_ptr<dht::DHTNode> dhtNode);

    /**
     * @brief Destructor
     */
    ~DHTMetadataProvider();

    /**
     * @brief Acquires metadata for an InfoHash
     * @param infoHash The InfoHash to acquire metadata for
     * @param callback The callback to call when metadata is acquired
     * @return True if the acquisition was started, false otherwise
     */
    bool acquireMetadata(
        const types::InfoHash& infoHash,
        std::function<void(bool success)> callback);

    /**
     * @brief Starts the provider
     * @return True if the provider was started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stops the provider
     */
    void stop();

    /**
     * @brief Checks if the provider is running
     * @return True if the provider is running, false otherwise
     */
    bool isRunning() const;

private:
    /**
     * @brief Represents a metadata acquisition task
     */
    struct MetadataTask {
        types::InfoHash infoHash;
        std::function<void(bool success)> callback;
        std::chrono::steady_clock::time_point startTime;
        int retryCount{0};
        std::atomic<bool> completed{false};
        std::set<std::string> transactionIds; // Transaction IDs for this task

        MetadataTask(const types::InfoHash& ih, std::function<void(bool success)> cb)
            : infoHash(ih), callback(cb), startTime(std::chrono::steady_clock::now()) {}
    };

    /**
     * @brief Sends a get_metadata query to a node
     * @param infoHash The InfoHash to get metadata for
     * @param nodeId The node ID to send the query to
     * @param task The metadata task
     * @return True if the query was sent, false otherwise
     */
    bool sendGetMetadataQuery(
        const types::InfoHash& infoHash,
        const types::NodeID& nodeId,
        std::shared_ptr<MetadataTask> task);

    /**
     * @brief Processes a metadata response
     * @param response The response
     * @param task The metadata task
     * @return True if the response was processed successfully, false otherwise
     */
    bool processMetadataResponse(
        const std::shared_ptr<bencode::BencodeValue>& response,
        std::shared_ptr<MetadataTask> task);

    /**
     * @brief Cleans up timed out tasks
     */
    void cleanupTimedOutTasks();

    /**
     * @brief Cleans up timed out tasks periodically
     */
    void cleanupTimedOutTasksPeriodically();

    // Dependencies
    std::shared_ptr<dht::DHTNode> m_dhtNode;
    std::shared_ptr<unified_event::EventBus> m_eventBus;

    // Active tasks
    std::unordered_map<std::string, std::shared_ptr<MetadataTask>> m_tasks;
    std::mutex m_tasksMutex;

    // Cleanup thread
    std::thread m_cleanupThread;
    std::atomic<bool> m_running{false};
    std::condition_variable m_cleanupCondition;
    std::mutex m_cleanupMutex;

    // Constants
    static constexpr int TASK_TIMEOUT_SECONDS = 60;
    static constexpr int CLEANUP_INTERVAL_SECONDS = 5;
    static constexpr int MAX_RETRY_COUNT = 3;
};

} // namespace dht_hunter::bittorrent::metadata
