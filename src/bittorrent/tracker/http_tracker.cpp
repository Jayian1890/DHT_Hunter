#include "dht_hunter/bittorrent/tracker/http_tracker.hpp"
#include "dht_hunter/bencode/bencode.hpp"
#include "dht_hunter/utility/network/network_utils.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <cstring>
#include <curl/curl.h>
#include <thread>

namespace dht_hunter::bittorrent::tracker {

// Callback function for cURL to write data
size_t curlWriteCallback(char* ptr, size_t size, size_t nmemb, std::string* data) {
    if (data == nullptr) {
        return 0;
    }
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

HTTPTracker::HTTPTracker(const std::string& url)
    : m_url(url), m_available(false), m_maxRetries(3), m_retryDelayMs(2000) {
    unified_event::logDebug("BitTorrent.HTTPTracker", "Created HTTP tracker: " + url);

    // Extract the scrape URL from the announce URL
    // The scrape URL is the announce URL with "announce" replaced with "scrape"
    size_t pos = m_url.find("announce");
    if (pos != std::string::npos) {
        m_scrapeURL = m_url.substr(0, pos) + "scrape" + m_url.substr(pos + 8);
    } else {
        m_scrapeURL = m_url;
    }

    // Initialize cURL globally if needed
    static std::once_flag curlInitFlag;
    std::call_once(curlInitFlag, []() {
        curl_global_init(CURL_GLOBAL_ALL);
        unified_event::logDebug("BitTorrent.HTTPTracker", "cURL globally initialized");
    });
}

HTTPTracker::~HTTPTracker() {
    unified_event::logDebug("BitTorrent.HTTPTracker", "Destroyed HTTP tracker: " + m_url);

    // Note: We don't need to call curl_global_cleanup() here because:
    // 1. It would affect other instances that might still be using cURL
    // 2. It will be automatically cleaned up when the program exits
}

std::string HTTPTracker::getURL() const {
    return m_url;
}

std::string HTTPTracker::getType() const {
    return "HTTP";
}

bool HTTPTracker::announce(
    const types::InfoHash& infoHash,
    uint16_t port,
    const std::string& event,
    std::function<void(bool success, const std::vector<types::EndPoint>& peers)> callback) {

    if (!callback) {
        unified_event::logWarning("BitTorrent.HTTPTracker", "Cannot announce - callback is null");
        return false;
    }

    unified_event::logDebug("BitTorrent.HTTPTracker", "Announcing to tracker: " + m_url + " for info hash: " + types::infoHashToString(infoHash));

    // Build the announce URL
    std::string announceURL = buildAnnounceURL(infoHash, port, event);
    unified_event::logDebug("BitTorrent.HTTPTracker", "Announce URL: " + announceURL);

    // Create a thread to handle the HTTP request asynchronously
    std::thread([this, announceURL, callback]() {
        // Implement retry logic
        bool success = false;
        std::vector<types::EndPoint> peers;
        std::string response;

        for (int retryCount = 0; retryCount <= m_maxRetries; retryCount++) {
            if (retryCount > 0) {
                unified_event::logInfo("BitTorrent.HTTPTracker", "Retrying announce request (attempt " +
                                     std::to_string(retryCount) + " of " + std::to_string(m_maxRetries) + ")");
                // Wait before retrying
                std::this_thread::sleep_for(std::chrono::milliseconds(m_retryDelayMs));
            }

            // Initialize cURL
            CURL* curl = curl_easy_init();
            if (!curl) {
                unified_event::logError("BitTorrent.HTTPTracker", "Failed to initialize cURL");
                continue; // Try again
            }

            // Clear response from previous attempts
            response.clear();

            // Set up the request
            curl_easy_setopt(curl, CURLOPT_URL, announceURL.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, utility::network::getUserAgent().c_str());

            // Set connection timeout
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

            // Perform the request
            CURLcode res = curl_easy_perform(curl);

            // Get HTTP response code
            long httpCode = 0;
            if (res == CURLE_OK) {
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
            }

            // Clean up cURL
            curl_easy_cleanup(curl);

            // Check for errors
            if (res != CURLE_OK) {
                unified_event::logWarning("BitTorrent.HTTPTracker", "cURL request failed: " +
                                       std::string(curl_easy_strerror(res)));
                continue; // Try again
            }

            // Check HTTP response code
            if (httpCode != 200) {
                unified_event::logWarning("BitTorrent.HTTPTracker", "HTTP request failed with code: " +
                                       std::to_string(httpCode));
                continue; // Try again
            }

            // Parse the response
            success = parseAnnounceResponse(response, peers);
            if (!success) {
                unified_event::logWarning("BitTorrent.HTTPTracker", "Failed to parse announce response");
                continue; // Try again
            }

            // Log success with peer count
            unified_event::logDebug("BitTorrent.HTTPTracker", "Successfully parsed announce response with " +
                                 std::to_string(peers.size()) + " peers");

            // If we got here, the request was successful
            break;
        }

        // Update tracker state
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_lastAnnounce = std::chrono::steady_clock::now();
            m_available = success;
        }

        // Call the callback with the peers
        callback(success, peers);
    }).detach();

    return true;
}

bool HTTPTracker::scrape(
    const std::vector<types::InfoHash>& infoHashes,
    std::function<void(bool success, const std::unordered_map<std::string, std::tuple<int, int, int>>& stats)> callback) {

    if (!callback) {
        unified_event::logWarning("BitTorrent.HTTPTracker", "Cannot scrape - callback is null");
        return false;
    }

    if (infoHashes.empty()) {
        unified_event::logWarning("BitTorrent.HTTPTracker", "Cannot scrape - no info hashes provided");
        callback(false, {});
        return false;
    }

    // Check if scrape URL is available
    if (m_scrapeURL.empty()) {
        unified_event::logWarning("BitTorrent.HTTPTracker", "Cannot scrape - no scrape URL available");
        callback(false, {});
        return false;
    }

    unified_event::logInfo("BitTorrent.HTTPTracker", "Scraping tracker: " + m_url + " for " + std::to_string(infoHashes.size()) + " info hashes");

    // Build the scrape URL
    std::string scrapeURL = buildScrapeURL(infoHashes);
    unified_event::logDebug("BitTorrent.HTTPTracker", "Scrape URL: " + scrapeURL);

    // Create a thread to handle the HTTP request asynchronously
    std::thread([this, scrapeURL, infoHashes, callback]() {
        // Implement retry logic
        bool success = false;
        std::unordered_map<std::string, std::tuple<int, int, int>> stats;
        std::string response;

        for (int retryCount = 0; retryCount <= m_maxRetries; retryCount++) {
            if (retryCount > 0) {
                unified_event::logInfo("BitTorrent.HTTPTracker", "Retrying scrape request (attempt " +
                                     std::to_string(retryCount) + " of " + std::to_string(m_maxRetries) + ")");
                // Wait before retrying
                std::this_thread::sleep_for(std::chrono::milliseconds(m_retryDelayMs));
            }

            // Initialize cURL
            CURL* curl = curl_easy_init();
            if (!curl) {
                unified_event::logError("BitTorrent.HTTPTracker", "Failed to initialize cURL");
                continue; // Try again
            }

            // Clear response from previous attempts
            response.clear();

            // Set up the request
            curl_easy_setopt(curl, CURLOPT_URL, scrapeURL.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, utility::network::getUserAgent().c_str());

            // Set connection timeout
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

            // Perform the request
            CURLcode res = curl_easy_perform(curl);

            // Get HTTP response code
            long httpCode = 0;
            if (res == CURLE_OK) {
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
            }

            // Clean up cURL
            curl_easy_cleanup(curl);

            // Check for errors
            if (res != CURLE_OK) {
                unified_event::logWarning("BitTorrent.HTTPTracker", "cURL request failed: " +
                                       std::string(curl_easy_strerror(res)));
                continue; // Try again
            }

            // Check HTTP response code
            if (httpCode != 200) {
                unified_event::logWarning("BitTorrent.HTTPTracker", "HTTP request failed with code: " +
                                       std::to_string(httpCode));
                continue; // Try again
            }

            // Parse the response
            stats.clear();
            success = parseScrapeResponse(response, stats);
            if (!success) {
                unified_event::logWarning("BitTorrent.HTTPTracker", "Failed to parse scrape response");
                continue; // Try again
            }

            // Log success with stats count
            unified_event::logDebug("BitTorrent.HTTPTracker", "Successfully parsed scrape response with " +
                                 std::to_string(stats.size()) + " info hashes");

            // If we got here, the request was successful
            break;
        }

        // Update tracker state
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_lastScrape = std::chrono::steady_clock::now();
            m_available = success;
        }

        // Call the callback with the stats
        callback(success, stats);
    }).detach();

    return true;
}

bool HTTPTracker::isAvailable() const {
    return m_available;
}

void HTTPTracker::updateAnnounceInterval(int interval) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_announceInterval = interval;
    unified_event::logDebug("BitTorrent.HTTPTracker", "Tracker announce interval: " + std::to_string(m_announceInterval) + " seconds");
}

void HTTPTracker::updateMinAnnounceInterval(int interval) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_minAnnounceInterval = interval;
    unified_event::logDebug("BitTorrent.HTTPTracker", "Tracker min announce interval: " + std::to_string(m_minAnnounceInterval) + " seconds");
}

std::string HTTPTracker::getStatus() const {
    std::stringstream ss;
    ss << "HTTP tracker: " << m_url;
    ss << ", Available: " << (m_available ? "Yes" : "No");

    if (m_available) {
        auto now = std::chrono::steady_clock::now();

        if (m_lastAnnounce.time_since_epoch().count() > 0) {
            auto lastAnnounce = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastAnnounce).count();
            ss << ", Last announce: " << lastAnnounce << " seconds ago";
        }

        if (m_lastScrape.time_since_epoch().count() > 0) {
            auto lastScrape = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastScrape).count();
            ss << ", Last scrape: " << lastScrape << " seconds ago";
        }
    }

    // Add scrape URL information
    if (!m_scrapeURL.empty()) {
        ss << ", Scrape URL: " << m_scrapeURL;
    } else {
        ss << ", No scrape URL available";
    }

    // Add interval information
    ss << ", Announce interval: " << m_announceInterval << " seconds";
    if (m_minAnnounceInterval > 0) {
        ss << ", Min interval: " << m_minAnnounceInterval << " seconds";
    }

    return ss.str();
}

std::string HTTPTracker::buildAnnounceURL(const types::InfoHash& infoHash, uint16_t port, const std::string& event) const {
    std::stringstream ss;
    ss << m_url;

    // Add the info hash
    ss << "?info_hash=";
    for (size_t i = 0; i < infoHash.size(); i++) {
        ss << "%" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(infoHash[i]);
    }

    // Add the peer ID (random 20-byte string)
    ss << "&peer_id=";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    for (int i = 0; i < 20; i++) {
        ss << "%" << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }

    // Add other parameters
    ss << "&port=" << port;
    ss << "&uploaded=0";
    ss << "&downloaded=0";
    ss << "&left=0";
    ss << "&compact=1";
    ss << "&numwant=50"; // Request up to 50 peers

    // Add the event if provided
    if (!event.empty()) {
        ss << "&event=" << event;
    }

    return ss.str();
}

std::string HTTPTracker::buildScrapeURL(const std::vector<types::InfoHash>& infoHashes) const {
    std::stringstream ss;
    ss << m_scrapeURL;

    // Add the info hashes
    for (size_t i = 0; i < infoHashes.size(); i++) {
        ss << (i == 0 ? "?" : "&") << "info_hash=";
        for (size_t j = 0; j < infoHashes[i].size(); j++) {
            ss << "%" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(infoHashes[i][j]);
        }
    }

    return ss.str();
}

bool HTTPTracker::parseAnnounceResponse(const std::string& response, std::vector<types::EndPoint>& peers) const {
    try {
        // Decode the response
        auto decoded = bencode::BencodeDecoder::decode(response);
        if (!decoded || !decoded->isDictionary()) {
            unified_event::logWarning("BitTorrent.HTTPTracker", "Invalid announce response - not a dictionary");
            return false;
        }

        // Check for failure
        auto failure = decoded->getString("failure reason");
        if (failure) {
            unified_event::logWarning("BitTorrent.HTTPTracker", "Announce failed: " + *failure);
            return false;
        }

        // Get the interval values
        auto interval = decoded->getInteger("interval");
        if (interval) {
            // Cannot modify member variables in a const method, so we'll use a mutable method
            const_cast<HTTPTracker*>(this)->updateAnnounceInterval(static_cast<int>(*interval));
        }

        auto minInterval = decoded->getInteger("min interval");
        if (minInterval) {
            // Cannot modify member variables in a const method, so we'll use a mutable method
            const_cast<HTTPTracker*>(this)->updateMinAnnounceInterval(static_cast<int>(*minInterval));
        }

        // Get the peers
        auto peersList = decoded->getList("peers");
        if (peersList) {
            // Dictionary model
            for (const auto& peerValue : *peersList) {
                auto peer = std::make_shared<bencode::BencodeValue>(*peerValue);

                if (!peer->isDictionary()) {
                    continue;
                }

                auto ip = peer->getString("ip");
                auto port = peer->getInteger("port");

                if (!ip || !port) {
                    continue;
                }

                peers.emplace_back(types::NetworkAddress(*ip), static_cast<uint16_t>(*port));
            }
        } else {
            // Binary model
            auto peersStr = decoded->getString("peers");
            if (!peersStr) {
                unified_event::logWarning("BitTorrent.HTTPTracker", "Invalid announce response - no peers");
                return false;
            }

            // Each peer is 6 bytes: 4 for IP, 2 for port
            const std::string& peersData = *peersStr;
            for (size_t i = 0; i + 5 < peersData.size(); i += 6) {
                // Extract IP
                uint32_t ip = 0;
                for (int j = 0; j < 4; j++) {
                    ip = (ip << 8) | static_cast<uint8_t>(peersData[static_cast<size_t>(i) + static_cast<size_t>(j)]);
                }

                // Extract port
                uint16_t port = static_cast<uint16_t>(
                    (static_cast<uint16_t>(static_cast<uint8_t>(peersData[static_cast<size_t>(i) + 4U])) << 8U) |
                    static_cast<uint16_t>(static_cast<uint8_t>(peersData[static_cast<size_t>(i) + 5U])));

                // Convert IP to string
                std::stringstream ss;
                ss << ((ip >> 24) & 0xFF) << "."
                   << ((ip >> 16) & 0xFF) << "."
                   << ((ip >> 8) & 0xFF) << "."
                   << (ip & 0xFF);

                peers.emplace_back(types::NetworkAddress(ss.str()), port);
            }
        }

        return true;
    } catch (const std::exception& e) {
        unified_event::logError("BitTorrent.HTTPTracker", "Error parsing announce response: " + std::string(e.what()));
        return false;
    }
}

bool HTTPTracker::parseScrapeResponse(const std::string& response, std::unordered_map<std::string, std::tuple<int, int, int>>& stats) const {
    try {
        // Decode the response
        auto decoded = bencode::BencodeDecoder::decode(response);
        if (!decoded || !decoded->isDictionary()) {
            unified_event::logWarning("BitTorrent.HTTPTracker", "Invalid scrape response - not a dictionary");
            return false;
        }

        // Check for failure
        auto failure = decoded->getString("failure reason");
        if (failure) {
            unified_event::logWarning("BitTorrent.HTTPTracker", "Scrape failed: " + *failure);
            return false;
        }

        // Get the files dictionary
        auto files = decoded->getDictionary("files");
        if (!files) {
            unified_event::logWarning("BitTorrent.HTTPTracker", "Invalid scrape response - no files dictionary");
            return false;
        }

        // Create a BencodeValue from the dictionary to access its methods
        auto filesValue = std::make_shared<bencode::BencodeValue>(*files);

        // Iterate over the files
        for (const auto& [key, value] : filesValue->getDict()) {
            auto file = std::make_shared<bencode::BencodeValue>(*value);

            if (!file->isDictionary()) {
                continue;
            }

            auto complete = file->getInteger("complete");
            auto incomplete = file->getInteger("incomplete");
            auto downloaded = file->getInteger("downloaded");

            if (!complete || !incomplete || !downloaded) {
                continue;
            }

            stats[key] = std::make_tuple(
                static_cast<int>(*complete),
                static_cast<int>(*incomplete),
                static_cast<int>(*downloaded)
            );
        }

        return true;
    } catch (const std::exception& e) {
        unified_event::logError("BitTorrent.HTTPTracker", "Error parsing scrape response: " + std::string(e.what()));
        return false;
    }
}

} // namespace dht_hunter::bittorrent::tracker
