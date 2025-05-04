#include "dht_hunter/bittorrent/tracker/http_tracker.hpp"
#include "dht_hunter/bencode/bencode.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <cstring>

namespace dht_hunter::bittorrent::tracker {

HTTPTracker::HTTPTracker(const std::string& url)
    : m_url(url), m_available(false) {
    unified_event::logInfo("BitTorrent.HTTPTracker", "Created HTTP tracker: " + url);

    // Extract the scrape URL from the announce URL
    // The scrape URL is the announce URL with "announce" replaced with "scrape"
    size_t pos = m_url.find("announce");
    if (pos != std::string::npos) {
        m_scrapeURL = m_url.substr(0, pos) + "scrape" + m_url.substr(pos + 8);
    } else {
        m_scrapeURL = m_url;
    }
}

HTTPTracker::~HTTPTracker() {
    unified_event::logInfo("BitTorrent.HTTPTracker", "Destroyed HTTP tracker: " + m_url);
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

    unified_event::logInfo("BitTorrent.HTTPTracker", "Announcing to tracker: " + m_url + " for info hash: " + types::infoHashToString(infoHash));

    // Build the announce URL
    std::string announceURL = buildAnnounceURL(infoHash, port, event);

    // In a real implementation, this would send an HTTP request to the tracker
    // For this example, we'll simulate a successful response with some fake peers
    std::vector<types::EndPoint> peers;
    peers.emplace_back(types::NetworkAddress("192.168.1.100"), 6881);
    peers.emplace_back(types::NetworkAddress("192.168.1.101"), 6882);
    peers.emplace_back(types::NetworkAddress("192.168.1.102"), 6883);

    // Update the last announce time
    m_lastAnnounce = std::chrono::steady_clock::now();
    m_available = true;

    // Call the callback with the peers
    callback(true, peers);
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

    unified_event::logInfo("BitTorrent.HTTPTracker", "Scraping tracker: " + m_url + " for " + std::to_string(infoHashes.size()) + " info hashes");

    // Build the scrape URL
    std::string scrapeURL = buildScrapeURL(infoHashes);

    // In a real implementation, this would send an HTTP request to the tracker
    // For this example, we'll simulate a successful response with some fake stats
    std::unordered_map<std::string, std::tuple<int, int, int>> stats;
    for (const auto& infoHash : infoHashes) {
        std::string infoHashStr = types::infoHashToString(infoHash);
        stats[infoHashStr] = std::make_tuple(10, 5, 2); // complete, incomplete, downloaded
    }

    // Update the last scrape time
    m_lastScrape = std::chrono::steady_clock::now();
    m_available = true;

    // Call the callback with the stats
    callback(true, stats);
    return true;
}

bool HTTPTracker::isAvailable() const {
    return m_available;
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
