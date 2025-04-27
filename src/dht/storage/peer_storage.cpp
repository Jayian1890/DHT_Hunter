#include "dht_hunter/dht/storage/peer_storage.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"

namespace dht_hunter::dht {

// Initialize static members
std::shared_ptr<PeerStorage> PeerStorage::s_instance = nullptr;
std::mutex PeerStorage::s_instanceMutex;

std::shared_ptr<PeerStorage> PeerStorage::getInstance(const DHTConfig& config) {
    std::lock_guard<std::mutex> lock(s_instanceMutex);

    if (!s_instance) {
        s_instance = std::shared_ptr<PeerStorage>(new PeerStorage(config));
    }

    return s_instance;
}

PeerStorage::PeerStorage(const DHTConfig& config)
    : m_config(config),
      m_running(false),
      m_eventBus(unified_event::EventBus::getInstance()),
      m_logger(event::Logger::forComponent("DHT.PeerStorage")) {
}

PeerStorage::~PeerStorage() {
    stop();

    // Clear the singleton instance
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (s_instance.get() == this) {
        s_instance.reset();
    }
}

bool PeerStorage::start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running) {
        m_logger.warning("Peer storage already running");
        return true;
    }

    m_running = true;

    // Start the cleanup thread
    m_cleanupThread = std::thread(&PeerStorage::cleanupExpiredPeersPeriodically, this);

    return true;
}

void PeerStorage::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running) {
        return;
    }

    m_running = false;

    // Wait for the cleanup thread to finish
    if (m_cleanupThread.joinable()) {
        m_cleanupThread.join();
    }

    m_logger.info("Peer storage stopped");
}

bool PeerStorage::isRunning() const {
    return m_running;
}

void PeerStorage::addPeer(const InfoHash& infoHash, const network::EndPoint& endpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if the info hash exists
    auto it = m_peers.find(infoHash);
    if (it == m_peers.end()) {
        // Create a new entry for the info hash
        m_peers[infoHash] = std::vector<TimestampedPeer>{TimestampedPeer(endpoint)};
        m_logger.debug("Added peer {} for info hash {}", endpoint.toString(), infoHashToString(infoHash));
        return;
    }

    // Check if the peer already exists
    auto& peers = it->second;
    for (auto& peer : peers) {
        if (peer.endpoint == endpoint) {
            // Update the timestamp
            peer.timestamp = std::chrono::steady_clock::now();
            m_logger.debug("Updated peer {} for info hash {}", endpoint.toString(), infoHashToString(infoHash));
            return;
        }
    }

    // Add the peer
    peers.emplace_back(endpoint);

    // If we have too many peers, remove the oldest one
    if (peers.size() > MAX_PEERS_PER_INFOHASH) {
        // Find the oldest peer
        auto oldest = peers.begin();
        for (auto peerIt = peers.begin() + 1; peerIt != peers.end(); ++peerIt) {
            if (peerIt->timestamp < oldest->timestamp) {
                oldest = peerIt;
            }
        }

        // Remove the oldest peer
        m_logger.debug("Removed peer {} for info hash {} (too many peers)", oldest->endpoint.toString(), infoHashToString(infoHash));
        peers.erase(oldest);
    }

    m_logger.debug("Added peer {} for info hash {}", endpoint.toString(), infoHashToString(infoHash));

    // Publish a peer discovered event
    // Convert EndPoint to NetworkAddress
    network::NetworkAddress networkAddress = endpoint.getAddress();
    auto event = std::make_shared<unified_event::PeerDiscoveredEvent>("DHT.PeerStorage", infoHash, networkAddress);
    m_eventBus->publish(event);
}

std::vector<network::EndPoint> PeerStorage::getPeers(const InfoHash& infoHash) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if the info hash exists
    auto it = m_peers.find(infoHash);
    if (it == m_peers.end()) {
        return {};
    }

    // Convert the timestamped peers to endpoints
    std::vector<network::EndPoint> endpoints;
    for (const auto& peer : it->second) {
        endpoints.push_back(peer.endpoint);
    }

    return endpoints;
}

size_t PeerStorage::getInfoHashCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_peers.size();
}

size_t PeerStorage::getPeerCount(const InfoHash& infoHash) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if the info hash exists
    auto it = m_peers.find(infoHash);
    if (it == m_peers.end()) {
        return 0;
    }

    return it->second.size();
}

size_t PeerStorage::getTotalPeerCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t count = 0;
    for (const auto& entry : m_peers) {
        count += entry.second.size();
    }

    return count;
}

void PeerStorage::cleanupExpiredPeers() {
    std::lock_guard<std::mutex> lock(m_mutex);

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

    m_logger.debug("Cleaned up expired peers, {} info hashes, {} total peers", m_peers.size(), getTotalPeerCount());
}

void PeerStorage::cleanupExpiredPeersPeriodically() {
    while (m_running) {
        // Sleep for a while
        std::this_thread::sleep_for(std::chrono::seconds(PEER_CLEANUP_INTERVAL));

        // Clean up expired peers
        cleanupExpiredPeers();
    }
}

} // namespace dht_hunter::dht
