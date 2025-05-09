#include "dht_hunter/dht/storage/peer_storage.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"

// Constants
constexpr int PEER_TTL = 1800;           // 30 minutes
constexpr int PEER_CLEANUP_INTERVAL = 30; // 30 seconds

namespace dht_hunter::dht {

// Initialize static members
std::shared_ptr<PeerStorage> PeerStorage::s_instance = nullptr;
std::mutex PeerStorage::s_instanceMutex;

std::shared_ptr<PeerStorage> PeerStorage::getInstance(const DHTConfig& config) {
    try {
        return utility::thread::withLock(s_instanceMutex, [&config]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<PeerStorage>(new PeerStorage(config));
            }
            return s_instance;
        }, "PeerStorage::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerStorage", e.what());
        return nullptr;
    }
}

PeerStorage::PeerStorage(const DHTConfig& config)
    : m_config(config),
      m_running(false),
      m_eventBus(unified_event::EventBus::getInstance()) {
}

PeerStorage::~PeerStorage() {
    stop();

    // Clear the singleton instance
    try {
        utility::thread::withLock(s_instanceMutex, [this]() {
            if (s_instance.get() == this) {
                s_instance.reset();
            }
        }, "PeerStorage::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerStorage", e.what());
    }
}

bool PeerStorage::start() {
    // Check if already running
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        // Already running
        return true;
    }

    // Start the cleanup thread
    m_cleanupThread = std::thread(&PeerStorage::cleanupExpiredPeersPeriodically, this);

    // Publish a system started event
    auto startedEvent = std::make_shared<unified_event::SystemStartedEvent>("DHT.PeerStorage");
    m_eventBus->publish(startedEvent);

    return true;
}

void PeerStorage::stop() {
    // Check if already stopped
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
        // Already stopped
        return;
    }

    // Join the cleanup thread
    if (m_cleanupThread.joinable()) {
        m_cleanupThread.join();
    }

    // Publish a system stopped event
    auto stoppedEvent = std::make_shared<unified_event::SystemStoppedEvent>("DHT.PeerStorage");
    m_eventBus->publish(stoppedEvent);
}

bool dht_hunter::dht::PeerStorage::isRunning() const {
    return m_running.load(std::memory_order_acquire);
}

void dht_hunter::dht::PeerStorage::addPeer(const InfoHash& infoHash, const EndPoint& endpoint) {
    std::string infoHashStr = types::infoHashToString(infoHash);
    try {
        utility::thread::withLock(m_mutex, [this, &infoHash, &endpoint, &infoHashStr]() {
            // Check if the info hash exists
            auto it = m_peers.find(infoHash);
            if (it == m_peers.end()) {
                // Create a new entry for the info hash
                m_peers[infoHash] = std::vector<TimestampedPeer>{TimestampedPeer(endpoint)};
                unified_event::logDebug("DHT.PeerStorage", "Added first peer for InfoHash: " + infoHashStr +
                                     ", peer: " + endpoint.toString());
                return;
            }

            // Check if the peer already exists
            auto& peers = it->second;
            for (auto& peer : peers) {
                if (peer.endpoint == endpoint) {
                    // Update the timestamp
                    peer.timestamp = std::chrono::steady_clock::now();
                    unified_event::logDebug("DHT.PeerStorage", "Updated timestamp for existing peer: " +
                                          endpoint.toString() + " for InfoHash: " + infoHashStr);
                    return;
                }
            }

            // Add the peer to the list
            peers.push_back(TimestampedPeer(endpoint));
            unified_event::logDebug("DHT.PeerStorage", "Added new peer for InfoHash: " + infoHashStr +
                                 ", peer: " + endpoint.toString() + ", total peers: " + std::to_string(peers.size()));
        }, "PeerStorage::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerStorage", e.what());
    }

    // Publish a peer discovered event outside the lock to avoid deadlocks
    try {
        // Convert EndPoint to NetworkAddress
        dht_hunter::network::NetworkAddress networkAddress = endpoint.getAddress();
        auto event = std::make_shared<unified_event::PeerDiscoveredEvent>("DHT.PeerStorage", infoHash, networkAddress);
        m_eventBus->publish(event);
    } catch (const std::exception& e) {
        unified_event::logError("DHT.PeerStorage", "Error publishing peer discovered event: " + std::string(e.what()));
    }
}

std::vector<EndPoint> dht_hunter::dht::PeerStorage::getPeers(const InfoHash& infoHash) {
    try {
        return utility::thread::withLock(m_mutex, [this, &infoHash]() {
            // Check if the info hash exists
            auto it = m_peers.find(infoHash);
            if (it == m_peers.end()) {
                return std::vector<EndPoint>{};
            }

            // Convert the timestamped peers to endpoints
            std::vector<EndPoint> endpoints;
            for (const auto& peer : it->second) {
                endpoints.push_back(peer.endpoint);
            }

            return endpoints;
        }, "PeerStorage::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerStorage", e.what());
        return std::vector<EndPoint>{};
    }
}

size_t dht_hunter::dht::PeerStorage::getInfoHashCount() const {
    try {
        return utility::thread::withLock(m_mutex, [this]() {
            return m_peers.size();
        }, "PeerStorage::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerStorage", e.what());
        return 0;
    }
}

size_t dht_hunter::dht::PeerStorage::getPeerCount(const InfoHash& infoHash) const {
    try {
        return utility::thread::withLock(m_mutex, [this, &infoHash]() {
            // Check if the info hash exists
            auto it = m_peers.find(infoHash);
            if (it == m_peers.end()) {
                return size_t(0);
            }

            return it->second.size();
        }, "PeerStorage::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerStorage", e.what());
        return 0;
    }
}

size_t dht_hunter::dht::PeerStorage::getTotalPeerCount() const {
    try {
        return utility::thread::withLock(m_mutex, [this]() {
            size_t count = 0;
            for (const auto& entry : m_peers) {
                count += entry.second.size();
            }

            return count;
        }, "PeerStorage::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerStorage", e.what());
        return 0;
    }
}

std::vector<InfoHash> dht_hunter::dht::PeerStorage::getAllInfoHashes() const {
    try {
        return utility::thread::withLock(m_mutex, [this]() {
            std::vector<InfoHash> infoHashes;
            infoHashes.reserve(m_peers.size());

            for (const auto& entry : m_peers) {
                infoHashes.push_back(entry.first);
            }

            return infoHashes;
        }, "PeerStorage::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerStorage", e.what());
        return std::vector<InfoHash>{};
    }
}

void dht_hunter::dht::PeerStorage::cleanupExpiredPeers() {
    try {
        utility::thread::withLock(m_mutex, [this]() {
            auto now = std::chrono::steady_clock::now();

            // Iterate over all info hashes
            for (auto it = m_peers.begin(); it != m_peers.end();) {
                auto& peers = it->second;

                // Remove expired peers
                peers.erase(
                    std::remove_if(peers.begin(), peers.end(),
                        [&now](const TimestampedPeer& peer) {
                            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - peer.timestamp).count();
                            return elapsed >= PEER_TTL;
                        }),
                    peers.end());

                // If there are no peers left, remove the info hash
                if (peers.empty()) {
                    it = m_peers.erase(it);
                } else {
                    ++it;
                }
            }
        }, "PeerStorage::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerStorage", e.what());
    }
}

void dht_hunter::dht::PeerStorage::logPeerStatistics() {
    try {
        utility::thread::withLock(m_mutex, [this]() {
            // Get total counts
            size_t totalInfoHashes = m_peers.size();
            size_t totalPeers = 0;
            size_t infoHashesWithMultiplePeers = 0;
            size_t maxPeersPerInfoHash = 0;

            // Count peers and analyze distribution
            std::unordered_map<size_t, size_t> peerCountDistribution; // Maps peer count to number of info hashes with that count

            for (const auto& entry : m_peers) {
                size_t peerCount = entry.second.size();
                totalPeers += peerCount;

                // Update max peers per info hash
                if (peerCount > maxPeersPerInfoHash) {
                    maxPeersPerInfoHash = peerCount;
                }

                // Count info hashes with multiple peers
                if (peerCount > 1) {
                    infoHashesWithMultiplePeers++;
                }

                // Update distribution
                peerCountDistribution[peerCount]++;
            }

            // Log overall statistics
            unified_event::logInfo("DHT.PeerStorage", "Peer Statistics - InfoHashes: " + std::to_string(totalInfoHashes) +
                                 ", Total Peers: " + std::to_string(totalPeers) +
                                 ", InfoHashes with multiple peers: " + std::to_string(infoHashesWithMultiplePeers) +
                                 ", Max peers per InfoHash: " + std::to_string(maxPeersPerInfoHash));

            // Log distribution
            std::string distributionStr = "Peer Count Distribution:";
            for (size_t i = 1; i <= std::min(maxPeersPerInfoHash, static_cast<size_t>(10)); i++) {
                distributionStr += " " + std::to_string(i) + ":" + std::to_string(peerCountDistribution[i]);
            }

            if (maxPeersPerInfoHash > 10) {
                distributionStr += " >10:";
                size_t countAbove10 = 0;
                for (size_t i = 11; i <= maxPeersPerInfoHash; i++) {
                    countAbove10 += peerCountDistribution[i];
                }
                distributionStr += std::to_string(countAbove10);
            }

            unified_event::logInfo("DHT.PeerStorage", distributionStr);

            // Log some examples of info hashes with multiple peers
            if (infoHashesWithMultiplePeers > 0) {
                size_t examplesLogged = 0;
                for (const auto& entry : m_peers) {
                    if (entry.second.size() > 1 && examplesLogged < 5) {
                        std::string infoHashStr = types::infoHashToString(entry.first);
                        unified_event::logInfo("DHT.PeerStorage", "InfoHash with multiple peers: " + infoHashStr +
                                             " has " + std::to_string(entry.second.size()) + " peers");
                        examplesLogged++;
                    }
                }
            }
        }, "PeerStorage::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerStorage", e.what());
    }
}

void dht_hunter::dht::PeerStorage::cleanupExpiredPeersPeriodically() {
    size_t iterationCount = 0;

    while (m_running.load(std::memory_order_acquire)) {
        // Sleep for a while
        std::this_thread::sleep_for(std::chrono::seconds(PEER_CLEANUP_INTERVAL));

        // Clean up expired peers
        cleanupExpiredPeers();

        // Log statistics every 10 iterations (approximately every 5 minutes if PEER_CLEANUP_INTERVAL is 30 seconds)
        iterationCount++;
        if (iterationCount % 10 == 0) {
            logPeerStatistics();
        }
    }
}

} // namespace dht_hunter::dht
