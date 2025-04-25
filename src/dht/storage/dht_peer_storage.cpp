#include "dht_hunter/dht/storage/dht_peer_storage.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include "dht_hunter/bencode/bencode.hpp"
#include <filesystem>
#include <fstream>
#include <algorithm>

DEFINE_COMPONENT_LOGGER("DHT", "PeerStorage")

namespace dht_hunter::dht {

DHTPeerStorage::DHTPeerStorage(const DHTNodeConfig& config)
    : m_config(config),
      m_peerCachePath(config.getPeerCachePath()),
      m_running(false) {
    getLogger()->info("Peer storage created");
}

DHTPeerStorage::~DHTPeerStorage() {
    stop();
}

bool DHTPeerStorage::start() {
    if (m_running) {
        getLogger()->warning("Peer storage already running");
        return false;
    }

    // Load the peer cache if it exists
    if (std::filesystem::exists(m_peerCachePath)) {
        getLogger()->info("Loading peer cache from {}", m_peerCachePath);
        if (!loadPeerCache(m_peerCachePath)) {
            getLogger()->warning("Failed to load peer cache from {}", m_peerCachePath);
        }
    } else {
        getLogger()->info("No peer cache file found at {}", m_peerCachePath);
    }

    // Start the cleanup thread
    m_running = true;
    m_cleanupThread = std::thread(&DHTPeerStorage::cleanupExpiredPeersPeriodically, this);
    m_saveThread = std::thread(&DHTPeerStorage::savePeerCachePeriodically, this);

    getLogger()->info("Peer storage started");
    return true;
}

void DHTPeerStorage::stop() {
    // Use atomic operation to prevent multiple stops
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false)) {
        // Already stopped or stopping
        return;
    }

    getLogger()->info("Stopping peer storage");

    // Save the peer cache before stopping
    savePeerCache(m_peerCachePath);

    // Join the cleanup thread with timeout
    if (m_cleanupThread.joinable()) {
        getLogger()->debug("Waiting for cleanup thread to join...");

        // Try to join with timeout
        auto joinStartTime = std::chrono::steady_clock::now();
        auto threadJoinTimeout = std::chrono::seconds(DEFAULT_THREAD_JOIN_TIMEOUT_SECONDS);

        std::thread tempThread([this]() {
            if (m_cleanupThread.joinable()) {
                m_cleanupThread.join();
            }
        });

        // Wait for the join thread with timeout
        if (tempThread.joinable()) {
            bool joined = false;
            while (!joined &&
                   std::chrono::steady_clock::now() - joinStartTime < threadJoinTimeout) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                joined = !m_cleanupThread.joinable();
            }

            if (joined) {
                tempThread.join();
                getLogger()->debug("Cleanup thread joined successfully");
            } else {
                // If we couldn't join, detach both threads to avoid blocking
                getLogger()->warning("Cleanup thread join timed out after {} seconds, detaching",
                              threadJoinTimeout.count());
                tempThread.detach();
                // We can't safely join the original thread now, so we detach it
                if (m_cleanupThread.joinable()) {
                    m_cleanupThread.detach();
                }
            }
        }
    }

    // Join the save thread with timeout
    if (m_saveThread.joinable()) {
        getLogger()->debug("Waiting for save thread to join...");

        // Try to join with timeout
        auto joinStartTime = std::chrono::steady_clock::now();
        auto threadJoinTimeout = std::chrono::seconds(DEFAULT_THREAD_JOIN_TIMEOUT_SECONDS);

        std::thread tempThread([this]() {
            if (m_saveThread.joinable()) {
                m_saveThread.join();
            }
        });

        // Wait for the join thread with timeout
        if (tempThread.joinable()) {
            bool joined = false;
            while (!joined &&
                   std::chrono::steady_clock::now() - joinStartTime < threadJoinTimeout) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                joined = !m_saveThread.joinable();
            }

            if (joined) {
                tempThread.join();
                getLogger()->debug("Save thread joined successfully");
            } else {
                // If we couldn't join, detach both threads to avoid blocking
                getLogger()->warning("Save thread join timed out after {} seconds, detaching",
                              threadJoinTimeout.count());
                tempThread.detach();
                // We can't safely join the original thread now, so we detach it
                if (m_saveThread.joinable()) {
                    m_saveThread.detach();
                }
            }
        }
    }

    getLogger()->info("Peer storage stopped");
}

bool DHTPeerStorage::isRunning() const {
    return m_running;
}

bool DHTPeerStorage::storePeer(const InfoHash& infoHash, const network::EndPoint& endpoint) {
    std::lock_guard<util::CheckedMutex> lock(m_peersMutex);

    // Check if we already have this peer for this info hash
    auto it = m_peers.find(infoHash);
    if (it != m_peers.end()) {
        // Check if we already have this peer
        for (auto& peer : it->second) {
            if (peer.endpoint == endpoint) {
                // Update the expiration time
                peer.expirationTime = std::chrono::steady_clock::now() + std::chrono::seconds(PEER_TTL);
                return true;
            }
        }

        // Check if we have reached the maximum number of peers for this info hash
        if (it->second.size() >= MAX_PEERS_PER_INFOHASH) {
            // Remove the oldest peer
            auto oldestIt = std::min_element(it->second.begin(), it->second.end(),
                [](const StoredPeer& a, const StoredPeer& b) {
                    return a.expirationTime < b.expirationTime;
                });

            if (oldestIt != it->second.end()) {
                *oldestIt = {endpoint, std::chrono::steady_clock::now() + std::chrono::seconds(PEER_TTL)};
                return true;
            }

            return false;
        }

        // Add the peer
        it->second.push_back({endpoint, std::chrono::steady_clock::now() + std::chrono::seconds(PEER_TTL)});
    } else {
        // Create a new entry for this info hash
        m_peers[infoHash] = {{endpoint, std::chrono::steady_clock::now() + std::chrono::seconds(PEER_TTL)}};
    }

    return true;
}

std::vector<network::EndPoint> DHTPeerStorage::getStoredPeers(const InfoHash& infoHash) const {
    std::lock_guard<util::CheckedMutex> lock(m_peersMutex);

    // Check if we have peers for this info hash
    auto it = m_peers.find(infoHash);
    if (it == m_peers.end()) {
        return {};
    }

    // Convert the stored peers to endpoints
    std::vector<network::EndPoint> endpoints;
    endpoints.reserve(it->second.size());

    for (const auto& peer : it->second) {
        endpoints.push_back(peer.endpoint);
    }

    return endpoints;
}

size_t DHTPeerStorage::getStoredPeerCount(const InfoHash& infoHash) const {
    std::lock_guard<util::CheckedMutex> lock(m_peersMutex);

    // Check if we have peers for this info hash
    auto it = m_peers.find(infoHash);
    if (it == m_peers.end()) {
        return 0;
    }

    return it->second.size();
}

size_t DHTPeerStorage::getTotalStoredPeerCount() const {
    std::lock_guard<util::CheckedMutex> lock(m_peersMutex);

    size_t count = 0;
    for (const auto& entry : m_peers) {
        count += entry.second.size();
    }

    return count;
}

size_t DHTPeerStorage::getInfoHashCount() const {
    std::lock_guard<util::CheckedMutex> lock(m_peersMutex);
    return m_peers.size();
}

bool DHTPeerStorage::savePeerCache(const std::string& filePath) const {
    // Create the directory if it doesn't exist
    std::filesystem::path path(filePath);
    std::filesystem::create_directories(path.parent_path());

    // Create a bencode dictionary to store the peer cache
    bencode::BencodeDict dict;

    // Add the peers to the dictionary
    {
        std::lock_guard<util::CheckedMutex> lock(m_peersMutex);

        for (const auto& entry : m_peers) {
            // Convert the info hash to a string
            std::string infoHashStr(reinterpret_cast<const char*>(entry.first.data()), entry.first.size());

            // Create a list of peers
            bencode::BencodeList peersList;

            for (const auto& peer : entry.second) {
                // Create a dictionary for the peer
                bencode::BencodeDict peerDict;

                // Add the endpoint
                auto ipValue = std::make_shared<bencode::BencodeValue>();
                ipValue->setString(peer.endpoint.getAddress().toString());
                peerDict["ip"] = ipValue;

                auto portValue = std::make_shared<bencode::BencodeValue>();
                portValue->setInteger(static_cast<int64_t>(peer.endpoint.getPort()));
                peerDict["port"] = portValue;

                // Add the expiration time
                auto now = std::chrono::steady_clock::now();
                auto ttl = std::chrono::duration_cast<std::chrono::seconds>(peer.expirationTime - now).count();

                auto ttlValue = std::make_shared<bencode::BencodeValue>();
                ttlValue->setInteger(static_cast<int64_t>(ttl));
                peerDict["ttl"] = ttlValue;

                // Create a BencodeValue for the peer dictionary
                auto peerDictValue = std::make_shared<bencode::BencodeValue>();
                for (const auto& [key, value] : peerDict) {
                    peerDictValue->set(key, value);
                }

                // Add the peer to the list
                peersList.push_back(peerDictValue);
            }

            // Create a BencodeValue for the peers list
            auto peersListValue = std::make_shared<bencode::BencodeValue>();
            peersListValue->setList(peersList);

            // Add the list to the dictionary
            dict[infoHashStr] = peersListValue;
        }
    }

    // Encode the dictionary
    std::string encoded = bencode::encode(dict);

    // Save the encoded dictionary to the file
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        getLogger()->error("Failed to open peer cache file for writing: {}", filePath);
        return false;
    }

    file.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
    if (!file) {
        getLogger()->error("Failed to write peer cache to file: {}", filePath);
        return false;
    }

    getLogger()->info("Peer cache saved to {}", filePath);
    return true;
}

bool DHTPeerStorage::loadPeerCache(const std::string& filePath) {
    // Check if the file exists
    if (!std::filesystem::exists(filePath)) {
        getLogger()->error("Peer cache file does not exist: {}", filePath);
        return false;
    }

    // Read the file
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        getLogger()->error("Failed to open peer cache file for reading: {}", filePath);
        return false;
    }

    // Get the file size
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read the file contents
    std::string encoded(static_cast<size_t>(size), '\0');
    if (!file.read(&encoded[0], size)) {
        getLogger()->error("Failed to read peer cache from file: {}", filePath);
        return false;
    }

    // Decode the bencode data
    bencode::BencodeParser parser;
    bencode::BencodeValue root;

    try {
        root = parser.parse(encoded);
    } catch (const std::exception& e) {
        getLogger()->error("Failed to parse peer cache: {}", e.what());
        return false;
    }

    // Check if the root is a dictionary
    if (!root.isDictionary()) {
        getLogger()->error("Peer cache is not a dictionary");
        return false;
    }

    // Get the dictionary
    const auto& dict = root.getDictionary();

    // Clear the current peers
    {
        std::lock_guard<util::CheckedMutex> lock(m_peersMutex);
        m_peers.clear();

        // Add the peers from the dictionary
        for (const auto& entry : dict) {
            // Convert the string to an info hash
            InfoHash infoHash;
            if (entry.first.size() != infoHash.size()) {
                getLogger()->warning("Invalid info hash size: {}", entry.first.size());
                continue;
            }

            std::copy(entry.first.begin(), entry.first.end(), infoHash.begin());

            // Check if the value is a list
            if (!entry.second->isList()) {
                getLogger()->warning("Peer list is not a list");
                continue;
            }

            // Get the list
            const auto& peersList = entry.second->getList();

            // Add the peers to the storage
            std::vector<StoredPeer> peers;

            for (const auto& peerValue : peersList) {
                // Check if the peer is a dictionary
                if (!peerValue->isDictionary()) {
                    getLogger()->warning("Peer is not a dictionary");
                    continue;
                }

                // Get the dictionary
                const auto& peerDict = peerValue->getDictionary();

                // Get the IP
                auto ipIt = peerDict.find("ip");
                if (ipIt == peerDict.end() || !ipIt->second->isString()) {
                    getLogger()->warning("Peer has no IP");
                    continue;
                }

                // Get the port
                auto portIt = peerDict.find("port");
                if (portIt == peerDict.end() || !portIt->second->isInteger()) {
                    getLogger()->warning("Peer has no port");
                    continue;
                }

                // Get the TTL
                auto ttlIt = peerDict.find("ttl");
                if (ttlIt == peerDict.end() || !ttlIt->second->isInteger()) {
                    getLogger()->warning("Peer has no TTL");
                    continue;
                }

                // Create the endpoint
                network::NetworkAddress address;
                if (!address.fromString(ipIt->second->getString())) {
                    getLogger()->warning("Invalid IP: {}", ipIt->second->getString());
                    continue;
                }

                network::EndPoint endpoint(address, static_cast<uint16_t>(portIt->second->getInteger()));

                // Create the expiration time
                auto ttl = ttlIt->second->getInteger();
                if (ttl <= 0) {
                    // Peer has expired
                    continue;
                }

                auto expirationTime = std::chrono::steady_clock::now() + std::chrono::seconds(ttl);

                // Add the peer
                peers.push_back({endpoint, expirationTime});
            }

            // Add the peers to the storage
            if (!peers.empty()) {
                m_peers[infoHash] = std::move(peers);
            }
        }
    }

    getLogger()->info("Peer cache loaded from {}", filePath);
    return true;
}

void DHTPeerStorage::cleanupExpiredPeers() {
    std::lock_guard<util::CheckedMutex> lock(m_peersMutex);

    // Get the current time
    auto now = std::chrono::steady_clock::now();

    // Iterate over all info hashes
    for (auto it = m_peers.begin(); it != m_peers.end();) {
        // Iterate over all peers for this info hash
        auto& peers = it->second;

        // Remove expired peers
        peers.erase(
            std::remove_if(peers.begin(), peers.end(),
                [now](const StoredPeer& peer) {
                    return peer.expirationTime <= now;
                }),
            peers.end());

        // Remove the info hash if it has no peers
        if (peers.empty()) {
            it = m_peers.erase(it);
        } else {
            ++it;
        }
    }
}

void DHTPeerStorage::cleanupExpiredPeersPeriodically() {
    getLogger()->debug("Starting periodic peer cleanup thread");

    while (m_running) {
        // Sleep for the cleanup interval
        std::this_thread::sleep_for(std::chrono::seconds(PEER_CLEANUP_INTERVAL));

        // Check if we should still be running
        if (!m_running) {
            break;
        }

        // Clean up expired peers
        cleanupExpiredPeers();
    }

    getLogger()->debug("Periodic peer cleanup thread exiting");
}

void DHTPeerStorage::savePeerCachePeriodically() {
    getLogger()->debug("Starting periodic peer cache save thread");

    while (m_running) {
        // Sleep for the save interval
        std::this_thread::sleep_for(std::chrono::seconds(TRANSACTION_SAVE_INTERVAL));

        // Check if we should still be running
        if (!m_running) {
            break;
        }

        // Save the peer cache
        savePeerCache(m_peerCachePath);
    }

    getLogger()->debug("Periodic peer cache save thread exiting");
}

} // namespace dht_hunter::dht
