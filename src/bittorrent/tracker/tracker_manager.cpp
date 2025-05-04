#include "dht_hunter/bittorrent/tracker/tracker_manager.hpp"
#include <sstream>
#include <algorithm>

namespace dht_hunter::bittorrent::tracker {

// Initialize static members
std::shared_ptr<TrackerManager> TrackerManager::s_instance = nullptr;
std::mutex TrackerManager::s_instanceMutex;

std::shared_ptr<TrackerManager> TrackerManager::getInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (!s_instance) {
        s_instance = std::shared_ptr<TrackerManager>(new TrackerManager());
    }
    return s_instance;
}

TrackerManager::TrackerManager() : m_initialized(false) {
    unified_event::logInfo("BitTorrent.TrackerManager", "Created tracker manager");
}

TrackerManager::~TrackerManager() {
    shutdown();
}

bool TrackerManager::initialize() {
    if (m_initialized) {
        return true;
    }

    unified_event::logInfo("BitTorrent.TrackerManager", "Initializing tracker manager");

    m_initialized = true;
    unified_event::logInfo("BitTorrent.TrackerManager", "Tracker manager initialized");
    return true;
}

void TrackerManager::shutdown() {
    if (!m_initialized) {
        return;
    }

    unified_event::logInfo("BitTorrent.TrackerManager", "Shutting down tracker manager");

    // Clear all trackers
    {
        std::lock_guard<std::mutex> lock(m_trackersMutex);
        m_trackers.clear();
    }

    m_initialized = false;
    unified_event::logInfo("BitTorrent.TrackerManager", "Tracker manager shut down");
}

bool TrackerManager::isInitialized() const {
    return m_initialized;
}

bool TrackerManager::addTracker(const std::string& url) {
    if (!m_initialized) {
        unified_event::logWarning("BitTorrent.TrackerManager", "Cannot add tracker - tracker manager not initialized");
        return false;
    }

    // Check if we already have this tracker
    {
        std::lock_guard<std::mutex> lock(m_trackersMutex);
        if (m_trackers.find(url) != m_trackers.end()) {
            return true;
        }
    }

    // Create the tracker
    auto tracker = createTracker(url);
    if (!tracker) {
        unified_event::logWarning("BitTorrent.TrackerManager", "Failed to create tracker: " + url);
        return false;
    }

    // Add the tracker to our list
    {
        std::lock_guard<std::mutex> lock(m_trackersMutex);
        m_trackers[url] = tracker;
    }

    unified_event::logInfo("BitTorrent.TrackerManager", "Added tracker: " + url);
    return true;
}

bool TrackerManager::removeTracker(const std::string& url) {
    if (!m_initialized) {
        unified_event::logWarning("BitTorrent.TrackerManager", "Cannot remove tracker - tracker manager not initialized");
        return false;
    }

    // Check if we have this tracker
    {
        std::lock_guard<std::mutex> lock(m_trackersMutex);
        if (m_trackers.find(url) == m_trackers.end()) {
            return false;
        }

        // Remove the tracker
        m_trackers.erase(url);
    }

    unified_event::logInfo("BitTorrent.TrackerManager", "Removed tracker: " + url);
    return true;
}

std::vector<std::shared_ptr<Tracker>> TrackerManager::getTrackers() const {
    std::vector<std::shared_ptr<Tracker>> trackers;

    {
        std::lock_guard<std::mutex> lock(m_trackersMutex);
        for (const auto& [url, tracker] : m_trackers) {
            trackers.push_back(tracker);
        }
    }

    return trackers;
}

std::shared_ptr<Tracker> TrackerManager::getTracker(const std::string& url) const {
    std::lock_guard<std::mutex> lock(m_trackersMutex);
    auto it = m_trackers.find(url);
    if (it != m_trackers.end()) {
        return it->second;
    }
    return nullptr;
}

bool TrackerManager::announceToAll(
    const types::InfoHash& infoHash,
    uint16_t port,
    const std::string& event,
    std::function<void(bool success, const std::vector<types::EndPoint>& peers)> callback) {

    if (!m_initialized) {
        unified_event::logWarning("BitTorrent.TrackerManager", "Cannot announce - tracker manager not initialized");
        if (callback) {
            callback(false, {});
        }
        return false;
    }

    if (!callback) {
        unified_event::logWarning("BitTorrent.TrackerManager", "Cannot announce - callback is null");
        return false;
    }

    unified_event::logInfo("BitTorrent.TrackerManager", "Announcing to all trackers for info hash: " + types::infoHashToString(infoHash));

    // Get all trackers
    std::vector<std::shared_ptr<Tracker>> trackers;
    {
        std::lock_guard<std::mutex> lock(m_trackersMutex);
        for (const auto& [url, tracker] : m_trackers) {
            trackers.push_back(tracker);
        }
    }

    if (trackers.empty()) {
        unified_event::logWarning("BitTorrent.TrackerManager", "No trackers available");
        callback(false, {});
        return false;
    }

    // Create a shared state for the callbacks
    struct SharedState {
        std::mutex mutex;
        int remaining;
        bool success;
        std::vector<types::EndPoint> peers;
    };
    auto state = std::make_shared<SharedState>();
    state->remaining = static_cast<int>(trackers.size());
    state->success = false;

    // Announce to each tracker
    for (const auto& tracker : trackers) {
        tracker->announce(infoHash, port, event, [state, callback](bool success, const std::vector<network::EndPoint>& peers) {
            bool allDone = false;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (success) {
                    state->success = true;
                    state->peers.insert(state->peers.end(), peers.begin(), peers.end());
                }
                state->remaining--;
                allDone = (state->remaining == 0);
            }

            if (allDone) {
                // Remove duplicate peers
                std::sort(state->peers.begin(), state->peers.end(), [](const network::EndPoint& a, const network::EndPoint& b) {
                    return a.toString() < b.toString();
                });
                state->peers.erase(std::unique(state->peers.begin(), state->peers.end(), [](const network::EndPoint& a, const network::EndPoint& b) {
                    return a.toString() == b.toString();
                }), state->peers.end());

                callback(state->success, state->peers);
            }
        });
    }

    return true;
}

bool TrackerManager::announceToTracker(
    const std::string& url,
    const types::InfoHash& infoHash,
    uint16_t port,
    const std::string& event,
    std::function<void(bool success, const std::vector<types::EndPoint>& peers)> callback) {

    if (!m_initialized) {
        unified_event::logWarning("BitTorrent.TrackerManager", "Cannot announce - tracker manager not initialized");
        if (callback) {
            callback(false, {});
        }
        return false;
    }

    if (!callback) {
        unified_event::logWarning("BitTorrent.TrackerManager", "Cannot announce - callback is null");
        return false;
    }

    // Get the tracker
    std::shared_ptr<Tracker> tracker;
    {
        std::lock_guard<std::mutex> lock(m_trackersMutex);
        auto it = m_trackers.find(url);
        if (it != m_trackers.end()) {
            tracker = it->second;
        }
    }

    if (!tracker) {
        unified_event::logWarning("BitTorrent.TrackerManager", "Tracker not found: " + url);
        callback(false, {});
        return false;
    }

    // Announce to the tracker
    return tracker->announce(infoHash, port, event, callback);
}

bool TrackerManager::scrapeAll(
    const std::vector<types::InfoHash>& infoHashes,
    std::function<void(bool success, const std::unordered_map<std::string, std::tuple<int, int, int>>& stats)> callback) {

    if (!m_initialized) {
        unified_event::logWarning("BitTorrent.TrackerManager", "Cannot scrape - tracker manager not initialized");
        if (callback) {
            callback(false, {});
        }
        return false;
    }

    if (!callback) {
        unified_event::logWarning("BitTorrent.TrackerManager", "Cannot scrape - callback is null");
        return false;
    }

    if (infoHashes.empty()) {
        unified_event::logWarning("BitTorrent.TrackerManager", "Cannot scrape - no info hashes provided");
        callback(false, {});
        return false;
    }

    unified_event::logInfo("BitTorrent.TrackerManager", "Scraping all trackers for " + std::to_string(infoHashes.size()) + " info hashes");

    // Get all trackers
    std::vector<std::shared_ptr<Tracker>> trackers;
    {
        std::lock_guard<std::mutex> lock(m_trackersMutex);
        for (const auto& [url, tracker] : m_trackers) {
            trackers.push_back(tracker);
        }
    }

    if (trackers.empty()) {
        unified_event::logWarning("BitTorrent.TrackerManager", "No trackers available");
        callback(false, {});
        return false;
    }

    // Create a shared state for the callbacks
    struct SharedState {
        std::mutex mutex;
        int remaining;
        bool success;
        std::unordered_map<std::string, std::tuple<int, int, int>> stats;
    };
    auto state = std::make_shared<SharedState>();
    state->remaining = static_cast<int>(trackers.size());
    state->success = false;

    // Scrape each tracker
    for (const auto& tracker : trackers) {
        tracker->scrape(infoHashes, [state, callback](bool success, const std::unordered_map<std::string, std::tuple<int, int, int>>& stats) {
            bool allDone = false;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (success) {
                    state->success = true;
                    for (const auto& [infoHash, stat] : stats) {
                        auto it = state->stats.find(infoHash);
                        if (it == state->stats.end()) {
                            state->stats[infoHash] = stat;
                        } else {
                            // Merge the stats (take the maximum values)
                            auto& [complete, incomplete, downloaded] = it->second;
                            auto& [newComplete, newIncomplete, newDownloaded] = stat;
                            complete = std::max(complete, newComplete);
                            incomplete = std::max(incomplete, newIncomplete);
                            downloaded = std::max(downloaded, newDownloaded);
                        }
                    }
                }
                state->remaining--;
                allDone = (state->remaining == 0);
            }

            if (allDone) {
                callback(state->success, state->stats);
            }
        });
    }

    return true;
}

bool TrackerManager::scrapeTracker(
    const std::string& url,
    const std::vector<types::InfoHash>& infoHashes,
    std::function<void(bool success, const std::unordered_map<std::string, std::tuple<int, int, int>>& stats)> callback) {

    if (!m_initialized) {
        unified_event::logWarning("BitTorrent.TrackerManager", "Cannot scrape - tracker manager not initialized");
        if (callback) {
            callback(false, {});
        }
        return false;
    }

    if (!callback) {
        unified_event::logWarning("BitTorrent.TrackerManager", "Cannot scrape - callback is null");
        return false;
    }

    if (infoHashes.empty()) {
        unified_event::logWarning("BitTorrent.TrackerManager", "Cannot scrape - no info hashes provided");
        callback(false, {});
        return false;
    }

    // Get the tracker
    std::shared_ptr<Tracker> tracker;
    {
        std::lock_guard<std::mutex> lock(m_trackersMutex);
        auto it = m_trackers.find(url);
        if (it != m_trackers.end()) {
            tracker = it->second;
        }
    }

    if (!tracker) {
        unified_event::logWarning("BitTorrent.TrackerManager", "Tracker not found: " + url);
        callback(false, {});
        return false;
    }

    // Scrape the tracker
    return tracker->scrape(infoHashes, callback);
}

std::string TrackerManager::getStatus() const {
    if (!m_initialized) {
        return "Not initialized";
    }

    std::stringstream ss;
    ss << "Initialized, " << m_trackers.size() << " trackers";
    return ss.str();
}

std::shared_ptr<Tracker> TrackerManager::createTracker(const std::string& url) {
    std::string type = getTrackerType(url);

    if (type == "HTTP") {
        return std::make_shared<HTTPTracker>(url);
    } else if (type == "UDP") {
        return std::make_shared<UDPTracker>(url);
    } else {
        unified_event::logWarning("BitTorrent.TrackerManager", "Unknown tracker type: " + type);
        return nullptr;
    }
}

std::string TrackerManager::getTrackerType(const std::string& url) const {
    if (url.substr(0, 7) == "http://" || url.substr(0, 8) == "https://") {
        return "HTTP";
    } else if (url.substr(0, 6) == "udp://") {
        return "UDP";
    } else {
        // Try to guess based on the URL
        if (url.find("://") == std::string::npos) {
            // No protocol specified, assume HTTP
            return "HTTP";
        } else {
            // Unknown protocol
            return "Unknown";
        }
    }
}

} // namespace dht_hunter::bittorrent::tracker
