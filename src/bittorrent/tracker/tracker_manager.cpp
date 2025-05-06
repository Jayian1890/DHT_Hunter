#include "dht_hunter/bittorrent/tracker/tracker_manager.hpp"
#include "dht_hunter/utility/config/configuration_manager.hpp"
#include "dht_hunter/utility/network/network_utils.hpp"
#include <sstream>
#include <algorithm>
#include <curl/curl.h>
#include <regex>
#include <chrono>

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
    unified_event::logDebug("BitTorrent.TrackerManager", "Created tracker manager");
}

TrackerManager::~TrackerManager() {
    shutdown();
}

bool TrackerManager::initialize() {
    if (m_initialized) {
        return true;
    }

    unified_event::logDebug("BitTorrent.TrackerManager", "Initializing tracker manager");

    // Mark as initialized first to avoid circular dependency issues
    m_initialized = true;
    unified_event::logDebug("BitTorrent.TrackerManager", "Tracker manager marked as initialized");

    // Load trackers from configured URLs
    auto configManager = utility::config::ConfigurationManager::getInstance();
    bool useTrackerListUrls = configManager->getBool("tracker.useTrackerListUrls", true);

    if (useTrackerListUrls) {
        int trackersLoaded = loadTrackersFromConfiguredUrls();
        unified_event::logInfo("BitTorrent.TrackerManager", "Loaded " + std::to_string(trackersLoaded) + " trackers");

        // Start the refresh thread
        m_refreshThreadRunning = true;
        m_refreshThread = std::thread([this]() {
            auto configManager = utility::config::ConfigurationManager::getInstance();
            int refreshIntervalHours = configManager->getInt("tracker.trackerListRefreshInterval", 24);

            while (m_refreshThreadRunning) {
                // Wait for the refresh interval or until shutdown
                std::unique_lock<std::mutex> lock(m_refreshMutex);
                if (m_refreshCondition.wait_for(lock, std::chrono::hours(refreshIntervalHours),
                    [this]() { return !m_refreshThreadRunning; })) {
                    // If we're here, we were signaled to stop
                    break;
                }

                // Refresh the tracker lists
                unified_event::logDebug("BitTorrent.TrackerManager", "Refreshing tracker lists");
                int trackersLoaded = loadTrackersFromConfiguredUrls();
                unified_event::logInfo("BitTorrent.TrackerManager", "Refreshed tracker lists, loaded " +
                                     std::to_string(trackersLoaded) + " trackers");
            }
        });
    }

    unified_event::logDebug("BitTorrent.TrackerManager", "Tracker manager initialization complete");
    return true;
}

void TrackerManager::shutdown() {
    if (!m_initialized) {
        return;
    }

    unified_event::logInfo("BitTorrent.TrackerManager", "Shutting down tracker manager");

    // Stop the refresh thread
    m_refreshThreadRunning = false;
    m_refreshCondition.notify_all();
    if (m_refreshThread.joinable()) {
        m_refreshThread.join();
    }

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
    // Check if we're initialized
    if (!m_initialized) {
        // During initialization, we're called from loadTrackersFromUrl which is called from initialize()
        // We'll allow this to proceed even though m_initialized is false
        unified_event::logDebug("BitTorrent.TrackerManager", "Adding tracker during initialization: " + url);
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

    unified_event::logDebug("BitTorrent.TrackerManager", "Added tracker: " + url);
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

    unified_event::logDebug("BitTorrent.TrackerManager", "Removed tracker: " + url);
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

// Callback function for cURL to write data
size_t writeCallback(char* ptr, size_t size, size_t nmemb, std::string* data) {
    if (data == nullptr) {
        return 0;
    }
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

int TrackerManager::loadTrackersFromConfiguredUrls() {
    auto configManager = utility::config::ConfigurationManager::getInstance();
    std::string udpTrackerListUrl = configManager->getString("tracker.udpTrackerListUrl",
        "https://github.com/ngosang/trackerslist/raw/refs/heads/master/trackers_all_udp.txt");
    std::string httpTrackerListUrl = configManager->getString("tracker.httpTrackerListUrl",
        "https://github.com/ngosang/trackerslist/raw/refs/heads/master/trackers_all_http.txt");

    int totalLoaded = 0;

    // Load UDP trackers
    if (!udpTrackerListUrl.empty()) {
        int udpTrackersLoaded = loadTrackersFromUrl(udpTrackerListUrl, "UDP");
        unified_event::logInfo("BitTorrent.TrackerManager", "Loaded " + std::to_string(udpTrackersLoaded) + " UDP trackers");
        totalLoaded += udpTrackersLoaded;
    }

    // Load HTTP trackers
    if (!httpTrackerListUrl.empty()) {
        int httpTrackersLoaded = loadTrackersFromUrl(httpTrackerListUrl, "HTTP");
        unified_event::logInfo("BitTorrent.TrackerManager", "Loaded " + std::to_string(httpTrackersLoaded) + " HTTP trackers");
        totalLoaded += httpTrackersLoaded;
    }

    return totalLoaded;
}

int TrackerManager::loadTrackersFromUrl(const std::string& url, const std::string& trackerType) {
    // Check if we're initialized
    if (!m_initialized) {
        // During initialization, we're called from initialize() method
        // We'll allow this to proceed even though m_initialized is false
        unified_event::logDebug("BitTorrent.TrackerManager", "Loading trackers during initialization");
    }

    unified_event::logDebug("BitTorrent.TrackerManager", "Loading " + trackerType + " trackers from " + url);

    // Fetch the content from the URL
    std::string content;
    if (!fetchUrl(url, content)) {
        unified_event::logWarning("BitTorrent.TrackerManager", "Failed to fetch tracker list from " + url);
        return 0;
    }

    // Parse the tracker URLs
    std::vector<std::string> trackerUrls = parseTrackerUrls(content, trackerType);
    unified_event::logDebug("BitTorrent.TrackerManager", "Parsed " + std::to_string(trackerUrls.size()) + " " + trackerType + " tracker URLs");

    // Add the trackers
    int trackersAdded = 0;
    for (const auto& trackerUrl : trackerUrls) {
        if (addTracker(trackerUrl)) {
            trackersAdded++;
        }
    }

    return trackersAdded;
}

bool TrackerManager::fetchUrl(const std::string& url, std::string& content) {
    // Initialize cURL
    CURL* curl = curl_easy_init();
    if (!curl) {
        unified_event::logError("BitTorrent.TrackerManager", "Failed to initialize cURL");
        return false;
    }

    // Set up the request
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &content);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, utility::network::getUserAgent().c_str());

    // Perform the request
    CURLcode res = curl_easy_perform(curl);

    // Check for errors
    bool success = (res == CURLE_OK);
    if (!success) {
        unified_event::logError("BitTorrent.TrackerManager", "cURL request failed: " + std::string(curl_easy_strerror(res)));
    }

    // Clean up
    curl_easy_cleanup(curl);

    return success;
}

std::vector<std::string> TrackerManager::parseTrackerUrls(const std::string& content, const std::string& trackerType) {
    std::vector<std::string> trackerUrls;

    // Split the content by lines
    std::istringstream iss(content);
    std::string line;

    while (std::getline(iss, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\n\r\f\v"));
        line.erase(line.find_last_not_of(" \t\n\r\f\v") + 1);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Check if the URL matches the tracker type
        if (trackerType == "UDP" && line.substr(0, 6) == "udp://") {
            trackerUrls.push_back(line);
        } else if (trackerType == "HTTP" && (line.substr(0, 7) == "http://" || line.substr(0, 8) == "https://")) {
            trackerUrls.push_back(line);
        }
    }

    return trackerUrls;
}

} // namespace dht_hunter::bittorrent::tracker
