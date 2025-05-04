#pragma once

#include "dht_hunter/types/info_hash.hpp"
#include "dht_hunter/types/endpoint.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace dht_hunter::bittorrent::tracker {

/**
 * @brief Interface for BitTorrent trackers
 *
 * This class provides an interface for BitTorrent trackers.
 */
class Tracker {
public:
    /**
     * @brief Destructor
     */
    virtual ~Tracker() = default;

    /**
     * @brief Gets the URL of the tracker
     * @return The URL of the tracker
     */
    virtual std::string getURL() const = 0;

    /**
     * @brief Gets the type of the tracker
     * @return The type of the tracker (HTTP, UDP, etc.)
     */
    virtual std::string getType() const = 0;

    /**
     * @brief Announces to the tracker
     * @param infoHash The info hash to announce
     * @param port The port to announce
     * @param event The event to announce (started, stopped, completed, empty)
     * @param callback The callback to call when the announce is complete
     * @return True if the announce was started, false otherwise
     */
    virtual bool announce(
        const types::InfoHash& infoHash,
        uint16_t port,
        const std::string& event,
        std::function<void(bool success, const std::vector<types::EndPoint>& peers)> callback) = 0;

    /**
     * @brief Scrapes the tracker
     * @param infoHashes The info hashes to scrape
     * @param callback The callback to call when the scrape is complete
     * @return True if the scrape was started, false otherwise
     */
    virtual bool scrape(
        const std::vector<types::InfoHash>& infoHashes,
        std::function<void(bool success, const std::unordered_map<std::string, std::tuple<int, int, int>>& stats)> callback) = 0;

    /**
     * @brief Checks if the tracker is available
     * @return True if the tracker is available, false otherwise
     */
    virtual bool isAvailable() const = 0;

    /**
     * @brief Gets the status of the tracker
     * @return A string describing the status
     */
    virtual std::string getStatus() const = 0;
};

} // namespace dht_hunter::bittorrent::tracker
