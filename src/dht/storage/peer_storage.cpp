#include "dht_hunter/dht/storage/peer_storage.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/utils/lock_utils.hpp"

namespace dht_hunter::dht {

// Initialize static members
std::shared_ptr<PeerStorage> PeerStorage::s_instance = nullptr;
std::mutex PeerStorage::s_instanceMutex;

std::shared_ptr<PeerStorage> PeerStorage::getInstance(const DHTConfig& config) {
    try {
        return utils::withLock(s_instanceMutex, [&config]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<PeerStorage>(new PeerStorage(config));
            }
            return s_instance;
        }, "PeerStorage::s_instanceMutex");
    } catch (const utils::LockTimeoutException& e) {
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
        utils::withLock(s_instanceMutex, [this]() {
            if (s_instance.get() == this) {
                s_instance.reset();
            }
        }, "PeerStorage::s_instanceMutex");
    } catch (const utils::LockTimeoutException& e) {
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

void dht_hunter::dht::PeerStorage::addPeer(const InfoHash& infoHash, const dht_hunter::network::EndPoint& endpoint) {
    try {
        utils::withLock(m_mutex, [this, &infoHash, &endpoint]() {
            // Check if the info hash exists
            auto it = m_peers.find(infoHash);
            if (it == m_peers.end()) {
                // Create a new entry for the info hash
                m_peers[infoHash] = std::vector<TimestampedPeer>{TimestampedPeer(endpoint)};
                return;
            }

            // Check if the peer already exists
            auto& peers = it->second;
            for (auto& peer : peers) {
                if (peer.endpoint == endpoint) {
                    // Update the timestamp
                    peer.timestamp = std::chrono::steady_clock::now();
                    return;
                }
            }

            // Add the peer to the list
            peers.push_back(TimestampedPeer(endpoint));
        }, "PeerStorage::m_mutex");
    } catch (const utils::LockTimeoutException& e) {
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

std::vector<dht_hunter::network::EndPoint> dht_hunter::dht::PeerStorage::getPeers(const InfoHash& infoHash) {
    try {
        return utils::withLock(m_mutex, [this, &infoHash]() {
            // Check if the info hash exists
            auto it = m_peers.find(infoHash);
            if (it == m_peers.end()) {
                return std::vector<dht_hunter::network::EndPoint>{};
            }

            // Convert the timestamped peers to endpoints
            std::vector<dht_hunter::network::EndPoint> endpoints;
            for (const auto& peer : it->second) {
                endpoints.push_back(peer.endpoint);
            }

            return endpoints;
        }, "PeerStorage::m_mutex");
    } catch (const utils::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerStorage", e.what());
        return std::vector<dht_hunter::network::EndPoint>{};
    }
}

size_t dht_hunter::dht::PeerStorage::getInfoHashCount() const {
    try {
        return utils::withLock(m_mutex, [this]() {
            return m_peers.size();
        }, "PeerStorage::m_mutex");
    } catch (const utils::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerStorage", e.what());
        return 0;
    }
}

size_t dht_hunter::dht::PeerStorage::getPeerCount(const InfoHash& infoHash) const {
    try {
        return utils::withLock(m_mutex, [this, &infoHash]() {
            // Check if the info hash exists
            auto it = m_peers.find(infoHash);
            if (it == m_peers.end()) {
                return size_t(0);
            }

            return it->second.size();
        }, "PeerStorage::m_mutex");
    } catch (const utils::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerStorage", e.what());
        return 0;
    }
}

size_t dht_hunter::dht::PeerStorage::getTotalPeerCount() const {
    try {
        return utils::withLock(m_mutex, [this]() {
            size_t count = 0;
            for (const auto& entry : m_peers) {
                count += entry.second.size();
            }

            return count;
        }, "PeerStorage::m_mutex");
    } catch (const utils::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerStorage", e.what());
        return 0;
    }
}

void dht_hunter::dht::PeerStorage::cleanupExpiredPeers() {
    try {
        utils::withLock(m_mutex, [this]() {
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
    } catch (const utils::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerStorage", e.what());
    }
}

void dht_hunter::dht::PeerStorage::cleanupExpiredPeersPeriodically() {
    while (m_running.load(std::memory_order_acquire)) {
        // Sleep for a while
        std::this_thread::sleep_for(std::chrono::seconds(PEER_CLEANUP_INTERVAL));

        // Clean up expired peers
        cleanupExpiredPeers();
    }
}

} // namespace dht_hunter::dht
