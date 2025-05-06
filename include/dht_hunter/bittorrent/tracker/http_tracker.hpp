#pragma once

#include "dht_hunter/bittorrent/tracker/tracker.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>

namespace dht_hunter::bittorrent::tracker {

/**
 * @brief HTTP BitTorrent tracker
 *
 * This class provides an implementation of the HTTP BitTorrent tracker protocol.
 */
class HTTPTracker : public Tracker {
public:
    /**
     * @brief Constructor
     * @param url The URL of the tracker
     */
    explicit HTTPTracker(const std::string& url);

    /**
     * @brief Destructor
     */
    ~HTTPTracker() override;

    /**
     * @brief Gets the URL of the tracker
     * @return The URL of the tracker
     */
    std::string getURL() const override;

    /**
     * @brief Gets the type of the tracker
     * @return The type of the tracker (HTTP)
     */
    std::string getType() const override;

    /**
     * @brief Announces to the tracker
     * @param infoHash The info hash to announce
     * @param port The port to announce
     * @param event The event to announce (started, stopped, completed, empty)
     * @param callback The callback to call when the announce is complete
     * @return True if the announce was started, false otherwise
     */
    bool announce(
        const types::InfoHash& infoHash,
        uint16_t port,
        const std::string& event,
        std::function<void(bool success, const std::vector<types::EndPoint>& peers)> callback) override;

    /**
     * @brief Scrapes the tracker
     * @param infoHashes The info hashes to scrape
     * @param callback The callback to call when the scrape is complete
     * @return True if the scrape was started, false otherwise
     */
    bool scrape(
        const std::vector<types::InfoHash>& infoHashes,
        std::function<void(bool success, const std::unordered_map<std::string, std::tuple<int, int, int>>& stats)> callback) override;

    /**
     * @brief Checks if the tracker is available
     * @return True if the tracker is available, false otherwise
     */
    bool isAvailable() const override;

    /**
     * @brief Gets the status of the tracker
     * @return A string describing the status
     */
    std::string getStatus() const override;

    /**
     * @brief Update the announce interval
     * @param interval The new interval in seconds
     */
    void updateAnnounceInterval(int interval);

    /**
     * @brief Update the minimum announce interval
     * @param interval The new interval in seconds
     */
    void updateMinAnnounceInterval(int interval);

private:
    /**
     * @brief Builds the announce URL
     * @param infoHash The info hash to announce
     * @param port The port to announce
     * @param event The event to announce
     * @return The announce URL
     */
    std::string buildAnnounceURL(const types::InfoHash& infoHash, uint16_t port, const std::string& event) const;

    /**
     * @brief Builds the scrape URL
     * @param infoHashes The info hashes to scrape
     * @return The scrape URL
     */
    std::string buildScrapeURL(const std::vector<types::InfoHash>& infoHashes) const;

    /**
     * @brief Parses the announce response
     * @param response The response from the tracker
     * @param peers The vector to fill with peers
     * @return True if the response was parsed successfully, false otherwise
     */
    bool parseAnnounceResponse(const std::string& response, std::vector<types::EndPoint>& peers) const;

    /**
     * @brief Parses the scrape response
     * @param response The response from the tracker
     * @param stats The map to fill with stats
     * @return True if the response was parsed successfully, false otherwise
     */
    bool parseScrapeResponse(const std::string& response, std::unordered_map<std::string, std::tuple<int, int, int>>& stats) const;

    std::string m_url;
    std::string m_scrapeURL;
    std::atomic<bool> m_available{false};
    std::chrono::steady_clock::time_point m_lastAnnounce;
    std::chrono::steady_clock::time_point m_lastScrape;
    std::mutex m_mutex;

    // Retry configuration
    int m_maxRetries;
    int m_retryDelayMs;

    // Tracker-provided intervals
    int m_announceInterval{1800}; // Default 30 minutes
    int m_minAnnounceInterval{900}; // Default 15 minutes
};

} // namespace dht_hunter::bittorrent::tracker
