#pragma once

#include "dht_hunter/bittorrent/tracker/tracker.hpp"
#include "dht_hunter/network/udp_client.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>
#include <condition_variable>

namespace dht_hunter::bittorrent::tracker {

/**
 * @brief UDP BitTorrent tracker
 *
 * This class provides an implementation of the UDP BitTorrent tracker protocol.
 */
class UDPTracker : public Tracker {
public:
    /**
     * @brief Constructor
     * @param url The URL of the tracker
     */
    explicit UDPTracker(const std::string& url);

    /**
     * @brief Destructor
     */
    ~UDPTracker() override;

    /**
     * @brief Gets the URL of the tracker
     * @return The URL of the tracker
     */
    std::string getURL() const override;

    /**
     * @brief Gets the type of the tracker
     * @return The type of the tracker (UDP)
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

private:
    /**
     * @brief Initializes the UDP client
     * @return True if initialization was successful, false otherwise
     */
    bool initializeUDPClient();

    /**
     * @brief Connects to the tracker
     * @param callback The callback to call when the connection is complete
     * @return True if the connection was started, false otherwise
     */
    bool connect(std::function<void(bool success, uint64_t connectionId)> callback);

    /**
     * @brief Sends an announce request
     * @param connectionId The connection ID
     * @param infoHash The info hash to announce
     * @param port The port to announce
     * @param event The event to announce
     * @param callback The callback to call when the announce is complete
     * @return True if the announce was started, false otherwise
     */
    bool sendAnnounce(
        uint64_t connectionId,
        const types::InfoHash& infoHash,
        uint16_t port,
        const std::string& event,
        std::function<void(bool success, const std::vector<network::EndPoint>& peers)> callback);

    /**
     * @brief Sends a scrape request
     * @param connectionId The connection ID
     * @param infoHashes The info hashes to scrape
     * @param callback The callback to call when the scrape is complete
     * @return True if the scrape was started, false otherwise
     */
    bool sendScrape(
        uint64_t connectionId,
        const std::vector<types::InfoHash>& infoHashes,
        std::function<void(bool success, const std::unordered_map<std::string, std::tuple<int, int, int>>& stats)> callback);

    /**
     * @brief Handles a connect response
     * @param data The response data
     * @param length The length of the response
     * @param callback The callback to call
     * @return True if the response was handled successfully, false otherwise
     */
    bool handleConnectResponse(
        const uint8_t* data,
        size_t length,
        std::function<void(bool success, uint64_t connectionId)> callback);

    /**
     * @brief Handles an announce response
     * @param data The response data
     * @param length The length of the response
     * @param callback The callback to call
     * @return True if the response was handled successfully, false otherwise
     */
    bool handleAnnounceResponse(
        const uint8_t* data,
        size_t length,
        std::function<void(bool success, const std::vector<network::EndPoint>& peers)> callback);

    /**
     * @brief Handles a scrape response
     * @param data The response data
     * @param length The length of the response
     * @param infoHashes The info hashes that were scraped
     * @param callback The callback to call
     * @return True if the response was handled successfully, false otherwise
     */
    bool handleScrapeResponse(
        const uint8_t* data,
        size_t length,
        const std::vector<types::InfoHash>& infoHashes,
        std::function<void(bool success, const std::unordered_map<std::string, std::tuple<int, int, int>>& stats)> callback);

    /**
     * @brief Generates a transaction ID
     * @return A transaction ID
     */
    uint32_t generateTransactionId();

    std::string m_url;
    std::string m_host;
    uint16_t m_port;
    std::atomic<bool> m_available{false};
    std::chrono::steady_clock::time_point m_lastAnnounce;
    std::chrono::steady_clock::time_point m_lastScrape;
    std::mutex m_mutex;

    std::shared_ptr<network::UDPClient> m_udpClient;
    std::atomic<bool> m_connected{false};
    uint64_t m_connectionId{0};
    std::chrono::steady_clock::time_point m_connectionTime;

    std::unordered_map<uint32_t, std::function<void(const uint8_t*, size_t)>> m_pendingRequests;
    std::mutex m_pendingRequestsMutex;

    // Constants
    static constexpr uint64_t CONNECT_MAGIC = 0x41727101980;
    static constexpr int CONNECTION_TIMEOUT_SECONDS = 60;
    static constexpr int REQUEST_TIMEOUT_SECONDS = 15;
    static constexpr int MAX_RETRIES = 3;
};

} // namespace dht_hunter::bittorrent::tracker
