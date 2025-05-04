#pragma once

#include "dht_hunter/bittorrent/tracker/tracker.hpp"
#include "dht_hunter/bittorrent/tracker/http_tracker.hpp"
#include "dht_hunter/bittorrent/tracker/udp_tracker.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

namespace dht_hunter::bittorrent::tracker {

/**
 * @brief Manager for BitTorrent trackers
 *
 * This class manages multiple BitTorrent trackers and provides a unified interface.
 */
class TrackerManager {
public:
    /**
     * @brief Gets the singleton instance
     * @return The singleton instance
     */
    static std::shared_ptr<TrackerManager> getInstance();

    /**
     * @brief Destructor
     */
    ~TrackerManager();

    /**
     * @brief Initializes the tracker manager
     * @return True if initialization was successful, false otherwise
     */
    bool initialize();

    /**
     * @brief Shuts down the tracker manager
     */
    void shutdown();

    /**
     * @brief Checks if the tracker manager is initialized
     * @return True if initialized, false otherwise
     */
    bool isInitialized() const;

    /**
     * @brief Adds a tracker
     * @param url The URL of the tracker
     * @return True if the tracker was added, false otherwise
     */
    bool addTracker(const std::string& url);

    /**
     * @brief Removes a tracker
     * @param url The URL of the tracker
     * @return True if the tracker was removed, false otherwise
     */
    bool removeTracker(const std::string& url);

    /**
     * @brief Gets all trackers
     * @return All trackers
     */
    std::vector<std::shared_ptr<Tracker>> getTrackers() const;

    /**
     * @brief Gets a tracker by URL
     * @param url The URL of the tracker
     * @return The tracker, or nullptr if not found
     */
    std::shared_ptr<Tracker> getTracker(const std::string& url) const;

    /**
     * @brief Announces to all trackers
     * @param infoHash The info hash to announce
     * @param port The port to announce
     * @param event The event to announce (started, stopped, completed, empty)
     * @param callback The callback to call when all announces are complete
     * @return True if the announces were started, false otherwise
     */
    bool announceToAll(
        const types::InfoHash& infoHash,
        uint16_t port,
        const std::string& event,
        std::function<void(bool success, const std::vector<types::EndPoint>& peers)> callback);

    /**
     * @brief Announces to a specific tracker
     * @param url The URL of the tracker
     * @param infoHash The info hash to announce
     * @param port The port to announce
     * @param event The event to announce (started, stopped, completed, empty)
     * @param callback The callback to call when the announce is complete
     * @return True if the announce was started, false otherwise
     */
    bool announceToTracker(
        const std::string& url,
        const types::InfoHash& infoHash,
        uint16_t port,
        const std::string& event,
        std::function<void(bool success, const std::vector<types::EndPoint>& peers)> callback);

    /**
     * @brief Scrapes all trackers
     * @param infoHashes The info hashes to scrape
     * @param callback The callback to call when all scrapes are complete
     * @return True if the scrapes were started, false otherwise
     */
    bool scrapeAll(
        const std::vector<types::InfoHash>& infoHashes,
        std::function<void(bool success, const std::unordered_map<std::string, std::tuple<int, int, int>>& stats)> callback);

    /**
     * @brief Scrapes a specific tracker
     * @param url The URL of the tracker
     * @param infoHashes The info hashes to scrape
     * @param callback The callback to call when the scrape is complete
     * @return True if the scrape was started, false otherwise
     */
    bool scrapeTracker(
        const std::string& url,
        const std::vector<types::InfoHash>& infoHashes,
        std::function<void(bool success, const std::unordered_map<std::string, std::tuple<int, int, int>>& stats)> callback);

    /**
     * @brief Gets the status of the tracker manager
     * @return A string describing the status
     */
    std::string getStatus() const;

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    TrackerManager();

    /**
     * @brief Creates a tracker from a URL
     * @param url The URL of the tracker
     * @return The tracker, or nullptr if the URL is invalid
     */
    std::shared_ptr<Tracker> createTracker(const std::string& url);

    /**
     * @brief Gets the type of a tracker URL
     * @param url The URL of the tracker
     * @return The type of the tracker (HTTP, UDP, etc.)
     */
    std::string getTrackerType(const std::string& url) const;

    // Static instance for singleton pattern
    static std::shared_ptr<TrackerManager> s_instance;
    static std::mutex s_instanceMutex;

    // Trackers
    std::unordered_map<std::string, std::shared_ptr<Tracker>> m_trackers;
    mutable std::mutex m_trackersMutex;
    std::atomic<bool> m_initialized{false};
};

} // namespace dht_hunter::bittorrent::tracker
