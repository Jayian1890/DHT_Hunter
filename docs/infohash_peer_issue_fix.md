# InfoHash Single-Peer Issue: Analysis and Implemented Solutions

## Problem Description

The DHT-Hunter application was only finding InfoHashes with a single peer. This limited the effectiveness of the application as popular torrents typically have multiple peers.

## Root Causes Analysis

After analyzing the codebase, several causes were identified and fixed:

1. **Limited DHT Crawling Depth**:
   - The crawler was not exploring the DHT network deeply enough
   - The crawler was stopping after finding the first peer for an InfoHash

2. **Peer Aggregation Issues**:
   - The program was finding multiple peers but not properly aggregating them
   - There was insufficient logging in the peer storage mechanism

3. **Tracker Communication Limitations**:
   - The UDP tracker implementation was not correctly processing all peers from responses
   - The tracker responses were not being fully parsed

4. **Metadata Acquisition Limitations**:
   - The metadata acquisition process was stopping after finding one valid peer

## Implemented Solutions

### 1. Enhanced PeerStorage Implementation

The PeerStorage class has been improved to better handle multiple peers per InfoHash with detailed logging:

```cpp
void PeerStorage::addPeer(const InfoHash& infoHash, const network::EndPoint& endpoint) {
    std::string infoHashStr = types::infoHashToString(infoHash);
    try {
        utility::thread::withLock(m_mutex, [this, &infoHash, &endpoint, &infoHashStr]() {
            // Check if the info hash exists
            auto it = m_peers.find(infoHash);
            if (it == m_peers.end()) {
                // Create a new entry for the info hash
                m_peers[infoHash] = std::vector<TimestampedPeer>{TimestampedPeer(endpoint)};
                unified_event::logInfo("DHT.PeerStorage", "Added first peer for InfoHash: " + infoHashStr +
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
            unified_event::logInfo("DHT.PeerStorage", "Added new peer for InfoHash: " + infoHashStr +
                                 ", peer: " + endpoint.toString() + ", total peers: " + std::to_string(peers.size()));
        }, "PeerStorage::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerStorage", e.what());
    }
}
```

Additionally, a new method was added to periodically log peer statistics:

```cpp
void PeerStorage::logPeerStatistics() {
    try {
        utility::thread::withLock(m_mutex, [this]() {
            // Get total counts
            size_t totalInfoHashes = m_peers.size();
            size_t totalPeers = 0;
            size_t infoHashesWithMultiplePeers = 0;
            size_t maxPeersPerInfoHash = 0;

            // Count peers and analyze distribution
            std::unordered_map<size_t, size_t> peerCountDistribution;

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

            // Log distribution and examples
            // ...
        }, "PeerStorage::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PeerStorage", e.what());
    }
}
```

### 2. Improved DHT Crawler

The DHT Crawler has been enhanced to continue searching for peers even after finding some:

```cpp
void Crawler::monitorInfoHashes() {
    // ... existing code ...

    // First lookup to find initial peers
    m_peerLookup->lookup(infoHash, [this, infoHash](const std::vector<network::EndPoint>& peers) {
        // ... process peers ...

        // Continue searching for more peers even after finding some
        if (m_running && !peers.empty()) {
            // Schedule a second lookup after a short delay to find more peers
            std::thread([this, infoHash, infoHashStr]() {
                // Wait a bit before doing another lookup
                std::this_thread::sleep_for(std::chrono::seconds(5));

                if (!m_running) return; // Check if still running

                // Do another lookup to find more peers
                m_peerLookup->lookup(infoHash, [this, infoHash, infoHashStr](const std::vector<network::EndPoint>& morePeers) {
                    // ... process additional peers ...
                });
            }).detach();
        }
    });
}
```

### 3. Enhanced UDP Tracker Response Handling

The UDP tracker implementation has been improved to correctly process all peers from responses:

```cpp
bool UDPTracker::handleAnnounceResponse(
    const uint8_t* data,
    size_t length,
    std::function<void(bool success, const std::vector<types::EndPoint>& peers)> callback) {

    // ... existing validation code ...

    // Extract ALL peers from the response
    std::vector<types::EndPoint> peers;
    size_t numPeers = (length - 20) / 6;

    unified_event::logInfo("BitTorrent.UDPTracker", "Received " + std::to_string(numPeers) + " peers from tracker");

    // Make sure we have enough data for all peers
    if (length >= 20 + numPeers * 6) {
        for (size_t i = 0; i < numPeers; i++) {
            size_t offset = 20 + i * 6;

            // Extract IP and port
            // ... extraction code ...

            // Skip invalid ports or IPs
            if (port == 0 || port > 65000 || ipStr == "0.0.0.0") {
                continue;
            }

            // Add the peer to the list
            peers.emplace_back(types::NetworkAddress(ipStr), port);
            unified_event::logDebug("BitTorrent.UDPTracker", "Received peer: " + ipStr + ":" + std::to_string(port));
        }
    }

    // Call the callback with ALL peers
    callback(true, peers);
    return true;
}
```

### 4. Enhanced Metadata Acquisition

The metadata acquisition process has been modified to try multiple peers and continue even after a successful acquisition:

```cpp
bool MetadataExchange::acquireMetadata(
    const types::InfoHash& infoHash,
    const std::vector<network::EndPoint>& peers,
    std::function<void(bool success)> callback,
    int maxConcurrentPeers) {

    // ... existing code ...

    // Increase the default max concurrent peers to improve chances of success
    maxConcurrentPeers = std::min(maxConcurrentPeers, MAX_CONCURRENT_PEERS_PER_INFOHASH);
    if (maxConcurrentPeers < 5 && peers.size() > 5) {
        maxConcurrentPeers = 5; // Try more peers concurrently
    }

    // Create a new task
    auto task = std::make_shared<AcquisitionTask>(infoHash, peers, callback, maxConcurrentPeers);

    // Set the task to continue after first success
    task->continueAfterFirstSuccess = true;

    // ... rest of the method ...
}

void MetadataExchange::handleSuccessfulAcquisition(std::shared_ptr<PeerConnection> connection) {
    // ... existing code ...

    // Mark the task as having at least one successful acquisition
    task->hasSucceeded = true;
    task->completedConnections++;

    // Call the callback if this is the first success
    if (!task->callbackCalled) {
        task->callbackCalled = true;
        if (task->callback) {
            task->callback(true);
        }
    }

    // If we should continue after first success, keep going with other peers
    if (task->continueAfterFirstSuccess) {
        // Try more peers if we have capacity
        if (task->activeConnections < task->maxConcurrentPeers && !task->remainingPeers.empty()) {
            tryMorePeers(task);
        }

        // ... complete the task only when appropriate ...
    }
}
```

### 5. Added Diagnostic Logging

Detailed logging has been added throughout the codebase to track peer counts:

1. **PeerStorage**: Added comprehensive logging of peer statistics
2. **UDP Tracker**: Enhanced logging of peers received from trackers
3. **DHT Crawler**: Added detailed logging of peer discovery
4. **Metadata Exchange**: Added logging for multiple peer connections

## Diagnostic Results

After implementing the solutions, the following improvements have been observed:

1. **Multiple Peers per InfoHash**:
   - The system now correctly finds and stores multiple peers per InfoHash
   - The periodic statistics logs show InfoHashes with multiple peers

2. **Improved Tracker Communication**:
   - The UDP tracker now correctly processes all peers from responses
   - Logs show multiple peers being received from trackers

3. **Enhanced Metadata Acquisition**:
   - The metadata acquisition process now tries multiple peers
   - Logs show successful metadata acquisition from different peers

## Conclusion

The InfoHash single-peer issue has been successfully resolved by implementing the proposed solutions. The DHT-Hunter application now correctly finds and stores multiple peers per InfoHash, which significantly improves its effectiveness for popular torrents.
