#include "dht_hunter/dht/dht_node.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include <algorithm>
#include <chrono>
#include <thread>

DEFINE_COMPONENT_LOGGER("DHT", "PeerStorage")

namespace dht_hunter::dht {
bool DHTNode::storePeer(const InfoHash& infoHash, const network::EndPoint& endpoint) {
    if (!m_running) {
        getLogger()->error("Cannot store peer: DHT node not running");
        return false;
    }
    // Create a stored peer
    StoredPeer peer;
    peer.endpoint = endpoint;
    peer.timestamp = std::chrono::steady_clock::now();
    // Get the info hash as a string
    std::string infoHashStr = nodeIDToString(infoHash);
    // Store the peer
    {
        std::lock_guard<std::mutex> lock(m_peersLock);
        // Get or create the peer set for this info hash
        auto& peerSet = m_peers[infoHashStr];
        // Check if we already have this peer
        auto it = peerSet.find(peer);
        if (it != peerSet.end()) {
            // Update the timestamp
            const_cast<StoredPeer&>(*it).timestamp = peer.timestamp;
            getLogger()->debug("Updated peer {} for info hash {}", endpoint.toString(), infoHashStr);
            return true;
        }
        // Check if we've reached the maximum number of peers for this info hash
        if (peerSet.size() >= MAX_PEERS_PER_INFOHASH) {
            // Find the oldest peer
            auto oldest = peerSet.begin();
            for (auto iter = peerSet.begin(); iter != peerSet.end(); ++iter) {
                if (iter->timestamp < oldest->timestamp) {
                    oldest = iter;
                }
            }
            // Remove the oldest peer
            peerSet.erase(oldest);
            getLogger()->debug("Removed oldest peer for info hash {} to make room for new peer", infoHashStr);
        }
        // Add the peer
        peerSet.insert(peer);
        getLogger()->debug("Stored peer {} for info hash {}", endpoint.toString(), infoHashStr);
    }
    return true;
}
std::vector<network::EndPoint> DHTNode::getStoredPeers(const InfoHash& infoHash) const {
    // Get the info hash as a string
    std::string infoHashStr = nodeIDToString(infoHash);
    // Get the peers
    std::vector<network::EndPoint> peers;
    {
        std::lock_guard<std::mutex> lock(m_peersLock);
        // Check if we have peers for this info hash
        auto it = m_peers.find(infoHashStr);
        if (it == m_peers.end()) {
            return peers;
        }
        // Get the peers
        const auto& peerSet = it->second;
        peers.reserve(peerSet.size());
        for (const auto& peer : peerSet) {
            peers.push_back(peer.endpoint);
        }
    }
    return peers;
}
size_t DHTNode::getStoredPeerCount(const InfoHash& infoHash) const {
    // Get the info hash as a string
    std::string infoHashStr = nodeIDToString(infoHash);
    // Get the peer count
    std::lock_guard<std::mutex> lock(m_peersLock);
    // Check if we have peers for this info hash
    auto it = m_peers.find(infoHashStr);
    if (it == m_peers.end()) {
        return 0;
    }
    return it->second.size();
}
size_t DHTNode::getTotalStoredPeerCount() const {
    std::lock_guard<std::mutex> lock(m_peersLock);
    size_t count = 0;
    for (const auto& [infoHash, peerSet] : m_peers) {
        count += peerSet.size();
    }
    return count;
}
size_t DHTNode::getInfoHashCount() const {
    std::lock_guard<std::mutex> lock(m_peersLock);
    return m_peers.size();
}
void DHTNode::cleanupExpiredPeers() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(m_peersLock);
    size_t removedCount = 0;
    std::vector<std::string> emptyInfoHashes;
    // Iterate through all info hashes
    for (auto& [infoHash, peerSet] : m_peers) {
        // Iterate through all peers for this info hash
        auto it = peerSet.begin();
        while (it != peerSet.end()) {
            // Check if the peer has expired
            auto age = std::chrono::duration_cast<std::chrono::seconds>(now - it->timestamp).count();
            if (age > PEER_TTL) {
                // Remove the peer
                it = peerSet.erase(it);
                removedCount++;
            } else {
                ++it;
            }
        }
        // Check if this info hash has no more peers
        if (peerSet.empty()) {
            emptyInfoHashes.push_back(infoHash);
        }
    }
    // Remove empty info hashes
    for (const auto& infoHash : emptyInfoHashes) {
        m_peers.erase(infoHash);
    }
    if (removedCount > 0 || !emptyInfoHashes.empty()) {
        getLogger()->debug("Cleaned up {} expired peers and {} empty info hashes",
                     removedCount, emptyInfoHashes.size());
    }
}
void DHTNode::peerCleanupThread() {
    getLogger()->debug("Peer cleanup thread started");
    while (m_running) {
        // Sleep for the cleanup interval
        std::this_thread::sleep_for(std::chrono::seconds(PEER_CLEANUP_INTERVAL));
        // Clean up expired peers
        if (m_running) {
            cleanupExpiredPeers();
        }
    }
    getLogger()->debug("Peer cleanup thread stopped");
}
} // namespace dht_hunter::dht
