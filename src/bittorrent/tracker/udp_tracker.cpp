#include "dht_hunter/bittorrent/tracker/udp_tracker.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

namespace dht_hunter::bittorrent::tracker {

UDPTracker::UDPTracker(const std::string& url)
    : m_url(url), m_available(false), m_connected(false), m_connectionId(0) {
    unified_event::logInfo("BitTorrent.UDPTracker", "Created UDP tracker: " + url);

    // Parse the URL to get the host and port
    // UDP tracker URLs are in the format: udp://host:port/announce
    std::string urlCopy = url;
    if (urlCopy.substr(0, 6) == "udp://") {
        urlCopy = urlCopy.substr(6);
    }

    size_t pos = urlCopy.find(':');
    if (pos != std::string::npos) {
        m_host = urlCopy.substr(0, pos);
        urlCopy = urlCopy.substr(pos + 1);

        pos = urlCopy.find('/');
        if (pos != std::string::npos) {
            m_port = static_cast<uint16_t>(std::stoi(urlCopy.substr(0, pos)));
        } else {
            m_port = static_cast<uint16_t>(std::stoi(urlCopy));
        }
    } else {
        pos = urlCopy.find('/');
        if (pos != std::string::npos) {
            m_host = urlCopy.substr(0, pos);
        } else {
            m_host = urlCopy;
        }
        m_port = 80;
    }

    unified_event::logInfo("BitTorrent.UDPTracker", "Parsed UDP tracker URL: " + m_url + " -> " + m_host + ":" + std::to_string(m_port));
}

UDPTracker::~UDPTracker() {
    unified_event::logInfo("BitTorrent.UDPTracker", "Destroyed UDP tracker: " + m_url);
}

std::string UDPTracker::getURL() const {
    return m_url;
}

std::string UDPTracker::getType() const {
    return "UDP";
}

bool UDPTracker::announce(
    const types::InfoHash& infoHash,
    uint16_t port,
    const std::string& event,
    std::function<void(bool success, const std::vector<types::EndPoint>& peers)> callback) {

    if (!callback) {
        unified_event::logWarning("BitTorrent.UDPTracker", "Cannot announce - callback is null");
        return false;
    }

    unified_event::logInfo("BitTorrent.UDPTracker", "Announcing to tracker: " + m_url + " for info hash: " + types::infoHashToString(infoHash));

    // Initialize the UDP client if needed
    if (!m_udpClient && !initializeUDPClient()) {
        unified_event::logWarning("BitTorrent.UDPTracker", "Failed to initialize UDP client");
        callback(false, {});
        return false;
    }

    // Connect to the tracker if needed
    if (!m_connected || std::chrono::steady_clock::now() - m_connectionTime > std::chrono::seconds(CONNECTION_TIMEOUT_SECONDS)) {
        connect([this, infoHash, port, event, callback](bool success, uint64_t connectionId) {
            if (!success) {
                unified_event::logWarning("BitTorrent.UDPTracker", "Failed to connect to tracker");
                callback(false, {});
                return;
            }

            // Send the announce request
            sendAnnounce(connectionId, infoHash, port, event, callback);
        });
        return true;
    }

    // Send the announce request
    return sendAnnounce(m_connectionId, infoHash, port, event, callback);
}

bool UDPTracker::scrape(
    const std::vector<types::InfoHash>& infoHashes,
    std::function<void(bool success, const std::unordered_map<std::string, std::tuple<int, int, int>>& stats)> callback) {

    if (!callback) {
        unified_event::logWarning("BitTorrent.UDPTracker", "Cannot scrape - callback is null");
        return false;
    }

    if (infoHashes.empty()) {
        unified_event::logWarning("BitTorrent.UDPTracker", "Cannot scrape - no info hashes provided");
        callback(false, {});
        return false;
    }

    unified_event::logInfo("BitTorrent.UDPTracker", "Scraping tracker: " + m_url + " for " + std::to_string(infoHashes.size()) + " info hashes");

    // Initialize the UDP client if needed
    if (!m_udpClient && !initializeUDPClient()) {
        unified_event::logWarning("BitTorrent.UDPTracker", "Failed to initialize UDP client");
        callback(false, {});
        return false;
    }

    // Connect to the tracker if needed
    if (!m_connected || std::chrono::steady_clock::now() - m_connectionTime > std::chrono::seconds(CONNECTION_TIMEOUT_SECONDS)) {
        connect([this, infoHashes, callback](bool success, uint64_t connectionId) {
            if (!success) {
                unified_event::logWarning("BitTorrent.UDPTracker", "Failed to connect to tracker");
                callback(false, {});
                return;
            }

            // Send the scrape request
            sendScrape(connectionId, infoHashes, callback);
        });
        return true;
    }

    // Send the scrape request
    return sendScrape(m_connectionId, infoHashes, callback);
}

bool UDPTracker::isAvailable() const {
    return m_available;
}

std::string UDPTracker::getStatus() const {
    std::stringstream ss;
    ss << "UDP tracker: " << m_url;
    ss << ", Available: " << (m_available ? "Yes" : "No");
    ss << ", Connected: " << (m_connected ? "Yes" : "No");

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

bool UDPTracker::initializeUDPClient() {
    try {
        m_udpClient = std::make_shared<network::UDPClient>();

        // Set up the data received callback
        m_udpClient->setMessageHandler([this](const std::vector<uint8_t>& data, const std::string& /*address*/, uint16_t /*port*/) {
            // Find the transaction ID in the response
            if (data.size() < 4) {
                return;
            }

            uint32_t transactionId = 0;
            for (int i = 0; i < 4; i++) {
                transactionId = (transactionId << 8) | static_cast<uint32_t>(data[static_cast<size_t>(i)]);
            }

            // Find the callback for this transaction
            std::function<void(const uint8_t*, size_t)> callback;
            {
                std::lock_guard<std::mutex> lock(m_pendingRequestsMutex);
                auto it = m_pendingRequests.find(transactionId);
                if (it != m_pendingRequests.end()) {
                    callback = it->second;
                    m_pendingRequests.erase(it);
                }
            }

            // Call the callback if found
            if (callback) {
                callback(data.data(), data.size());
            }
        });

        // Start the UDP client
        if (!m_udpClient->start()) {
            unified_event::logWarning("BitTorrent.UDPTracker", "Failed to start UDP client");
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        unified_event::logError("BitTorrent.UDPTracker", "Error initializing UDP client: " + std::string(e.what()));
        return false;
    }
}

bool UDPTracker::connect(std::function<void(bool success, uint64_t connectionId)> callback) {
    if (!m_udpClient) {
        unified_event::logWarning("BitTorrent.UDPTracker", "Cannot connect - UDP client not initialized");
        callback(false, 0);
        return false;
    }

    // Generate a transaction ID
    uint32_t transactionId = generateTransactionId();

    // Build the connect request
    // Connect request format:
    // 64-bit integer: connection_id (0x41727101980 for connect)
    // 32-bit integer: action (0 for connect)
    // 32-bit integer: transaction_id
    std::vector<uint8_t> request(16);
    uint64_t connectMagic = CONNECT_MAGIC;
    uint32_t action = 0;

    // Write the connection ID (big-endian)
    for (int i = 0; i < 8; i++) {
        request[static_cast<size_t>(7 - i)] = static_cast<uint8_t>((connectMagic >> (i * 8)) & 0xFF);
    }

    // Write the action (big-endian)
    for (int i = 0; i < 4; i++) {
        request[static_cast<size_t>(11 - i)] = static_cast<uint8_t>((action >> (i * 8)) & 0xFF);
    }

    // Write the transaction ID (big-endian)
    for (int i = 0; i < 4; i++) {
        request[static_cast<size_t>(15 - i)] = static_cast<uint8_t>((transactionId >> (i * 8)) & 0xFF);
    }

    // Register the callback for this transaction
    {
        std::lock_guard<std::mutex> lock(m_pendingRequestsMutex);
        m_pendingRequests[transactionId] = [this, callback](const uint8_t* data, size_t length) {
            handleConnectResponse(data, length, callback);
        };
    }

    // Send the request
    if (m_udpClient->send(request, m_host, m_port) < 0) {
        unified_event::logWarning("BitTorrent.UDPTracker", "Failed to send connect request");

        // Remove the callback
        {
            std::lock_guard<std::mutex> lock(m_pendingRequestsMutex);
            m_pendingRequests.erase(transactionId);
        }

        callback(false, 0);
        return false;
    }

    return true;
}

bool UDPTracker::sendAnnounce(
    uint64_t connectionId,
    const types::InfoHash& infoHash,
    uint16_t port,
    const std::string& event,
    std::function<void(bool success, const std::vector<types::EndPoint>& peers)> callback) {

    if (!m_udpClient) {
        unified_event::logWarning("BitTorrent.UDPTracker", "Cannot announce - UDP client not initialized");
        callback(false, {});
        return false;
    }

    // Generate a transaction ID
    uint32_t transactionId = generateTransactionId();

    // Build the announce request
    // Announce request format:
    // 64-bit integer: connection_id
    // 32-bit integer: action (1 for announce)
    // 32-bit integer: transaction_id
    // 20-byte string: info_hash
    // 20-byte string: peer_id
    // 64-bit integer: downloaded
    // 64-bit integer: left
    // 64-bit integer: uploaded
    // 32-bit integer: event
    // 32-bit integer: IP address (0 for default)
    // 32-bit integer: key
    // 32-bit integer: num_want (-1 for default)
    // 16-bit integer: port
    std::vector<uint8_t> request(98);
    uint32_t action = 1;
    uint32_t eventValue = 0;

    // Convert the event string to a value
    if (event == "started") {
        eventValue = 2;
    } else if (event == "completed") {
        eventValue = 1;
    } else if (event == "stopped") {
        eventValue = 3;
    }

    // Write the connection ID (big-endian)
    for (int i = 0; i < 8; i++) {
        request[static_cast<size_t>(7 - i)] = static_cast<uint8_t>((connectionId >> (i * 8)) & 0xFF);
    }

    // Write the action (big-endian)
    for (int i = 0; i < 4; i++) {
        request[static_cast<size_t>(11 - i)] = static_cast<uint8_t>((action >> (i * 8)) & 0xFF);
    }

    // Write the transaction ID (big-endian)
    for (int i = 0; i < 4; i++) {
        request[static_cast<size_t>(15 - i)] = static_cast<uint8_t>((transactionId >> (i * 8)) & 0xFF);
    }

    // Write the info hash
    std::copy(infoHash.begin(), infoHash.end(), request.begin() + 16);

    // Write the peer ID (random 20-byte string)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    for (int i = 0; i < 20; i++) {
        request[static_cast<size_t>(36 + i)] = static_cast<uint8_t>(dis(gen));
    }

    // Write downloaded, left, uploaded (all 0)
    for (int i = 56; i < 80; i++) {
        request[static_cast<size_t>(i)] = 0;
    }

    // Write the event (big-endian)
    for (int i = 0; i < 4; i++) {
        request[static_cast<size_t>(83 - i)] = static_cast<uint8_t>((eventValue >> (i * 8)) & 0xFF);
    }

    // Write IP address (0 for default)
    for (int i = 84; i < 88; i++) {
        request[static_cast<size_t>(i)] = 0;
    }

    // Write key (random 32-bit integer)
    uint32_t key = static_cast<uint32_t>(dis(gen));
    for (int i = 0; i < 4; i++) {
        request[static_cast<size_t>(91 - i)] = static_cast<uint8_t>((key >> (i * 8)) & 0xFF);
    }

    // Write num_want (-1 for default)
    for (int i = 0; i < 4; i++) {
        request[static_cast<size_t>(95 - i)] = 0xFF;
    }

    // Write port (big-endian)
    request[96U] = static_cast<uint8_t>((port >> 8) & 0xFF);
    request[97U] = static_cast<uint8_t>(port & 0xFF);

    // Register the callback for this transaction
    {
        std::lock_guard<std::mutex> lock(m_pendingRequestsMutex);
        m_pendingRequests[transactionId] = [this, callback](const uint8_t* data, size_t length) {
            handleAnnounceResponse(data, length, callback);
        };
    }

    // Send the request
    if (m_udpClient->send(request, m_host, m_port) < 0) {
        unified_event::logWarning("BitTorrent.UDPTracker", "Failed to send announce request");

        // Remove the callback
        {
            std::lock_guard<std::mutex> lock(m_pendingRequestsMutex);
            m_pendingRequests.erase(transactionId);
        }

        callback(false, {});
        return false;
    }

    return true;
}

bool UDPTracker::sendScrape(
    uint64_t connectionId,
    const std::vector<types::InfoHash>& infoHashes,
    std::function<void(bool success, const std::unordered_map<std::string, std::tuple<int, int, int>>& stats)> callback) {

    if (!m_udpClient) {
        unified_event::logWarning("BitTorrent.UDPTracker", "Cannot scrape - UDP client not initialized");
        callback(false, {});
        return false;
    }

    // Generate a transaction ID
    uint32_t transactionId = generateTransactionId();

    // Build the scrape request
    // Scrape request format:
    // 64-bit integer: connection_id
    // 32-bit integer: action (2 for scrape)
    // 32-bit integer: transaction_id
    // N * 20-byte string: info_hash (one or more)
    std::vector<uint8_t> request(16 + infoHashes.size() * 20);
    uint32_t action = 2;

    // Write the connection ID (big-endian)
    for (int i = 0; i < 8; i++) {
        request[static_cast<size_t>(7 - i)] = static_cast<uint8_t>((connectionId >> (i * 8)) & 0xFF);
    }

    // Write the action (big-endian)
    for (int i = 0; i < 4; i++) {
        request[static_cast<size_t>(11 - i)] = static_cast<uint8_t>((action >> (i * 8)) & 0xFF);
    }

    // Write the transaction ID (big-endian)
    for (int i = 0; i < 4; i++) {
        request[static_cast<size_t>(15 - i)] = static_cast<uint8_t>((transactionId >> (i * 8)) & 0xFF);
    }

    // Write the info hashes
    for (size_t i = 0; i < infoHashes.size(); i++) {
        std::copy(infoHashes[i].begin(), infoHashes[i].end(), request.begin() + static_cast<std::ptrdiff_t>(16U + i * 20U));
    }

    // Register the callback for this transaction
    {
        std::lock_guard<std::mutex> lock(m_pendingRequestsMutex);
        m_pendingRequests[transactionId] = [this, callback, infoHashes](const uint8_t* data, size_t length) {
            handleScrapeResponse(data, length, infoHashes, callback);
        };
    }

    // Send the request
    if (m_udpClient->send(request, m_host, m_port) < 0) {
        unified_event::logWarning("BitTorrent.UDPTracker", "Failed to send scrape request");

        // Remove the callback
        {
            std::lock_guard<std::mutex> lock(m_pendingRequestsMutex);
            m_pendingRequests.erase(transactionId);
        }

        callback(false, {});
        return false;
    }

    return true;
}

bool UDPTracker::handleConnectResponse(
    const uint8_t* data,
    size_t length,
    std::function<void(bool success, uint64_t connectionId)> callback) {

    if (length < 16) {
        unified_event::logWarning("BitTorrent.UDPTracker", "Invalid connect response - too short");
        callback(false, 0);
        return false;
    }

    // Connect response format:
    // 32-bit integer: action (0 for connect)
    // 32-bit integer: transaction_id
    // 64-bit integer: connection_id

    // Check the action
    uint32_t action = 0;
    for (int i = 0; i < 4; i++) {
        action = (action << 8) | static_cast<uint32_t>(data[i]);
    }

    if (action != 0) {
        unified_event::logWarning("BitTorrent.UDPTracker", "Invalid connect response - wrong action: " + std::to_string(action));
        callback(false, 0);
        return false;
    }

    // Extract the connection ID
    uint64_t connectionId = 0;
    for (int i = 0; i < 8; i++) {
        connectionId = (connectionId << 8) | static_cast<uint64_t>(data[8U + static_cast<size_t>(i)]);
    }

    // Store the connection ID and time
    m_connectionId = connectionId;
    m_connectionTime = std::chrono::steady_clock::now();
    m_connected = true;
    m_available = true;

    unified_event::logInfo("BitTorrent.UDPTracker", "Connected to tracker: " + m_url);

    // Call the callback
    callback(true, connectionId);
    return true;
}

bool UDPTracker::handleAnnounceResponse(
    const uint8_t* data,
    size_t length,
    std::function<void(bool success, const std::vector<types::EndPoint>& peers)> callback) {

    if (length < 20) {
        unified_event::logWarning("BitTorrent.UDPTracker", "Invalid announce response - too short");
        callback(false, {});
        return false;
    }

    // Announce response format:
    // 32-bit integer: action (1 for announce)
    // 32-bit integer: transaction_id
    // 32-bit integer: interval
    // 32-bit integer: leechers
    // 32-bit integer: seeders
    // N * 6 bytes: peers (4 for IP, 2 for port)

    // Check the action
    uint32_t action = 0;
    for (int i = 0; i < 4; i++) {
        action = (action << 8) | static_cast<uint32_t>(data[i]);
    }

    if (action != 1) {
        unified_event::logWarning("BitTorrent.UDPTracker", "Invalid announce response - wrong action: " + std::to_string(action));
        callback(false, {});
        return false;
    }

    // Extract the interval, leechers, and seeders
    uint32_t interval = 0;
    for (int i = 0; i < 4; i++) {
        interval = (interval << 8) | static_cast<uint32_t>(data[8U + static_cast<size_t>(i)]);
    }

    uint32_t leechers = 0;
    for (int i = 0; i < 4; i++) {
        leechers = (leechers << 8) | static_cast<uint32_t>(data[12U + static_cast<size_t>(i)]);
    }

    uint32_t seeders = 0;
    for (int i = 0; i < 4; i++) {
        seeders = (seeders << 8) | static_cast<uint32_t>(data[16U + static_cast<size_t>(i)]);
    }

    // Extract the peers
    std::vector<types::EndPoint> peers;
    for (size_t i = 20; i + 5 < length; i += 6) {
        // Extract IP
        uint32_t ip = 0;
        for (int j = 0; j < 4; j++) {
            ip = (ip << 8) | static_cast<uint32_t>(data[static_cast<size_t>(i) + static_cast<size_t>(j)]);
        }

        // Extract port
        uint16_t port = static_cast<uint16_t>(
            (static_cast<uint16_t>(data[i + 4U]) << 8U) | static_cast<uint16_t>(data[i + 5U]));

        // Convert IP to string
        std::stringstream ss;
        ss << ((ip >> 24) & 0xFF) << "."
           << ((ip >> 16) & 0xFF) << "."
           << ((ip >> 8) & 0xFF) << "."
           << (ip & 0xFF);

        peers.emplace_back(types::NetworkAddress(ss.str()), port);
    }

    // Update the last announce time
    m_lastAnnounce = std::chrono::steady_clock::now();

    unified_event::logInfo("BitTorrent.UDPTracker", "Announce successful: " + m_url + ", interval: " + std::to_string(interval) + ", leechers: " + std::to_string(leechers) + ", seeders: " + std::to_string(seeders) + ", peers: " + std::to_string(peers.size()));

    // Call the callback
    callback(true, peers);
    return true;
}

bool UDPTracker::handleScrapeResponse(
    const uint8_t* data,
    size_t length,
    const std::vector<types::InfoHash>& infoHashes,
    std::function<void(bool success, const std::unordered_map<std::string, std::tuple<int, int, int>>& stats)> callback) {

    if (length < 8 + infoHashes.size() * 12) {
        unified_event::logWarning("BitTorrent.UDPTracker", "Invalid scrape response - too short");
        callback(false, {});
        return false;
    }

    // Scrape response format:
    // 32-bit integer: action (2 for scrape)
    // 32-bit integer: transaction_id
    // N * 12 bytes: scrape info (3 * 32-bit integers: seeders, completed, leechers)

    // Check the action
    uint32_t action = 0;
    for (int i = 0; i < 4; i++) {
        action = (action << 8) | static_cast<uint32_t>(data[i]);
    }

    if (action != 2) {
        unified_event::logWarning("BitTorrent.UDPTracker", "Invalid scrape response - wrong action: " + std::to_string(action));
        callback(false, {});
        return false;
    }

    // Extract the scrape info
    std::unordered_map<std::string, std::tuple<int, int, int>> stats;
    for (size_t i = 0; i < infoHashes.size(); i++) {
        // Extract seeders, completed, and leechers
        uint32_t seeders = 0;
        for (int j = 0; j < 4; j++) {
            seeders = (seeders << 8) | static_cast<uint32_t>(data[8U + i * 12U + static_cast<size_t>(j)]);
        }

        uint32_t completed = 0;
        for (int j = 0; j < 4; j++) {
            completed = (completed << 8) | static_cast<uint32_t>(data[8U + i * 12U + 4U + static_cast<size_t>(j)]);
        }

        uint32_t leechers = 0;
        for (int j = 0; j < 4; j++) {
            leechers = (leechers << 8) | static_cast<uint32_t>(data[8U + i * 12U + 8U + static_cast<size_t>(j)]);
        }

        // Add to the stats
        std::string infoHashStr = types::infoHashToString(infoHashes[i]);
        stats[infoHashStr] = std::make_tuple(static_cast<int>(seeders), static_cast<int>(leechers), static_cast<int>(completed));
    }

    // Update the last scrape time
    m_lastScrape = std::chrono::steady_clock::now();

    unified_event::logInfo("BitTorrent.UDPTracker", "Scrape successful: " + m_url + ", info hashes: " + std::to_string(infoHashes.size()));

    // Call the callback
    callback(true, stats);
    return true;
}

uint32_t UDPTracker::generateTransactionId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);
    return dis(gen);
}

} // namespace dht_hunter::bittorrent::tracker
