#include "dht_hunter/dht/core/persistence_manager.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"

#include "dht_hunter/utility/hash/hash_utils.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace dht_hunter::dht {


namespace fs = std::filesystem;

// Static instance initialization
std::shared_ptr<PersistenceManager> PersistenceManager::s_instance = nullptr;
std::mutex PersistenceManager::s_instanceMutex;

std::shared_ptr<PersistenceManager> PersistenceManager::getInstance(const std::string& configDir) {
    try {
        return utility::thread::withLock(s_instanceMutex, [&configDir]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<PersistenceManager>(new PersistenceManager(configDir));
            }
            return s_instance;
        }, "PersistenceManager::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PersistenceManager", e.what());
        return nullptr;
    }
}

PersistenceManager::PersistenceManager(const std::string& configDir)
    : m_configDir(configDir),
      m_routingTablePath(configDir + "/routing_table.dat"),
      m_peerStoragePath(configDir + "/peer_storage.dat"),
      m_running(false),
      m_saveInterval(1) { // Save every 1 minute

    unified_event::logDebug("DHT.PersistenceManager", "Initializing with config directory: " + configDir);
    createConfigDir();
}

PersistenceManager::~PersistenceManager() {
    stop();

    // Clear the singleton instance
    try {
        utility::thread::withLock(s_instanceMutex, [this]() {
            if (s_instance.get() == this) {
                s_instance.reset();
            }
        }, "PersistenceManager::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PersistenceManager", e.what());
    }
}

bool PersistenceManager::start(std::shared_ptr<RoutingTable> routingTable, std::shared_ptr<PeerStorage> peerStorage) {
    bool shouldLoadData = false;

    try {
        // First, set up the routing table and peer storage while holding the lock
        if (!utility::thread::withLock(m_mutex, [this, routingTable, peerStorage, &shouldLoadData]() {
            if (m_running) {
                return true;
            }

            m_routingTable = routingTable;
            m_peerStorage = peerStorage;

            if (!m_routingTable || !m_peerStorage) {
                unified_event::logError("DHT.PersistenceManager", "Cannot start: routing table or peer storage is null");
                return false;
            }

            // Mark that we should load data after releasing the lock
            shouldLoadData = true;
            return true;
        }, "PersistenceManager::m_mutex")) {
            return false;
        }
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PersistenceManager", e.what());
        return false;
    }

    // Load data from disk (outside the lock)
    if (shouldLoadData) {
        loadFromDisk();
    }

    try {
        // Now start the save thread while holding the lock again
        return utility::thread::withLock(m_mutex, [this]() {
            if (m_running) {
                return true;
            }

            // Start the save thread
            m_running = true;
            m_saveThread = std::thread(&PersistenceManager::savePeriodically, this);

            unified_event::logDebug("DHT.PersistenceManager", "Started");
            return true;
        }, "PersistenceManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PersistenceManager", e.what());
        return false;
    }
}

void PersistenceManager::stop() {
    bool wasRunning = false;

    try {
        utility::thread::withLock(m_mutex, [this, &wasRunning]() {
            if (!m_running) {
                return;
            }

            // Mark that we were running (for final save after releasing lock)
            wasRunning = true;

            // Stop the save thread
            m_running = false;

            // Join the thread while holding the lock to ensure thread safety
            if (m_saveThread.joinable()) {
                m_saveThread.join();
            }
        }, "PersistenceManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PersistenceManager", e.what());
        return;
    }

    // If we were running, perform a final save AFTER releasing the lock
    if (wasRunning) {
        // Save data before fully stopping
        unified_event::logDebug("DHT.PersistenceManager", "Performing final save before shutdown");
        saveNow();
        unified_event::logDebug("DHT.PersistenceManager", "Stopped");
    }
}

bool PersistenceManager::isRunning() const {
    return m_running;
}

bool PersistenceManager::saveNow() {
    // First, check if we have valid objects to save
    std::shared_ptr<RoutingTable> routingTable;
    std::shared_ptr<PeerStorage> peerStorage;
    std::string routingTablePath;

    try {
        utility::thread::withLock(m_mutex, [this, &routingTable, &peerStorage, &routingTablePath]() {
            // Make local copies of the shared pointers while holding the lock
            routingTable = m_routingTable;
            peerStorage = m_peerStorage;
            routingTablePath = m_routingTablePath;
        }, "PersistenceManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PersistenceManager", e.what());
        return false;
    }

    // Check if we have valid objects to save (outside the lock)
    if (!routingTable || !peerStorage) {
        unified_event::logError("DHT.PersistenceManager", "Cannot save: routing table or peer storage is null");
        return false;
    }

    bool success = true;

    // Save routing table (outside the lock)
    unified_event::logInfo("DHT.PersistenceManager", "Saving routing table with " + std::to_string(routingTable->getNodeCount()) + " nodes to " + routingTablePath);
    if (!routingTable->saveToFile(routingTablePath)) {
        unified_event::logError("DHT.PersistenceManager", "Failed to save routing table to " + routingTablePath);
        success = false;
    } else {
        unified_event::logInfo("DHT.PersistenceManager", "Successfully saved routing table to " + routingTablePath);
    }

    // Get the peer storage path
    std::string peerStoragePath;
    try {
        utility::thread::withLock(m_mutex, [this, &peerStoragePath]() {
            peerStoragePath = m_peerStoragePath;
        }, "PersistenceManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PersistenceManager", e.what());
        // Continue with the routing table success status
        return success;
    }

    // Save peer storage (outside the lock)
    try {
        // Get all info hashes and their peers
        size_t infoHashCount = peerStorage->getInfoHashCount();
        size_t totalPeerCount = peerStorage->getTotalPeerCount();

        // Get all info hashes from the peer storage
        std::vector<InfoHash> infoHashes = peerStorage->getAllInfoHashes();

        unified_event::logInfo("DHT.PersistenceManager", "Saving " + std::to_string(infoHashCount) + " info hashes with " +
                                std::to_string(totalPeerCount) + " total peers to " + peerStoragePath);

        // Open file for binary writing
        std::ofstream file(peerStoragePath, std::ios::binary);
        if (file.is_open()) {
            // Write file format version
            uint32_t version = 1;
            file.write(reinterpret_cast<const char*>(&version), sizeof(version));

            // Write number of info hashes
            uint32_t numInfoHashes = static_cast<uint32_t>(infoHashes.size());
            file.write(reinterpret_cast<const char*>(&numInfoHashes), sizeof(numInfoHashes));

            // For each info hash, write its peers
            for (const auto& infoHash : infoHashes) {
                // Write the info hash (20 bytes)
                file.write(reinterpret_cast<const char*>(infoHash.data()), static_cast<std::streamsize>(infoHash.size()));

                // Get peers for this info hash
                auto peers = peerStorage->getPeers(infoHash);

                // Write number of peers for this info hash
                uint32_t numPeers = static_cast<uint32_t>(peers.size());
                file.write(reinterpret_cast<const char*>(&numPeers), sizeof(numPeers));

                // For each peer, write its address and port
                for (const auto& peer : peers) {
                    // Write IP address as string
                    std::string ipAddress = peer.getAddress().toString();
                    uint16_t ipLength = static_cast<uint16_t>(ipAddress.length());
                    file.write(reinterpret_cast<const char*>(&ipLength), sizeof(ipLength));
                    file.write(ipAddress.c_str(), static_cast<std::streamsize>(ipLength));

                    // Write port
                    uint16_t port = peer.getPort();
                    file.write(reinterpret_cast<const char*>(&port), sizeof(port));
                }
            }

            file.close();

            // Log success with details
            if (infoHashes.empty()) {
                unified_event::logInfo("DHT.PersistenceManager", "Saved empty peer storage to " + peerStoragePath);
            } else {
                // Get file size
                std::ifstream checkFile(peerStoragePath, std::ios::binary | std::ios::ate);
                std::streamsize fileSize = checkFile.tellg();
                checkFile.close();

                unified_event::logInfo("DHT.PersistenceManager", "Successfully saved " + std::to_string(infoHashes.size()) +
                                      " info hashes with " + std::to_string(totalPeerCount) + " peers to " + peerStoragePath +
                                      " (" + std::to_string(fileSize) + " bytes)");
            }
        } else {
            unified_event::logError("DHT.PersistenceManager", "Failed to open file for writing: " + peerStoragePath);
            success = false;
        }
    } catch (const std::exception& e) {
        unified_event::logError("DHT.PersistenceManager", "Exception while saving peer storage: " + std::string(e.what()));
        success = false;
    }

    return success;
}

bool PersistenceManager::loadFromDisk() {
    // First, check if we have valid objects to load into
    std::shared_ptr<RoutingTable> routingTable;
    std::shared_ptr<PeerStorage> peerStorage;
    std::string routingTablePath;

    try {
        utility::thread::withLock(m_mutex, [this, &routingTable, &peerStorage, &routingTablePath]() {
            // Make local copies of the shared pointers while holding the lock
            routingTable = m_routingTable;
            peerStorage = m_peerStorage;
            routingTablePath = m_routingTablePath;
        }, "PersistenceManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PersistenceManager", e.what());
        return false;
    }

    // Check if we have valid objects to load into (outside the lock)
    if (!routingTable || !peerStorage) {
        unified_event::logError("DHT.PersistenceManager", "Cannot load: routing table or peer storage is null");
        return false;
    }

    bool success = true;

    // Load routing table (outside the lock)
    if (fs::exists(routingTablePath)) {
        unified_event::logInfo("DHT.PersistenceManager", "Found routing table file at: " + routingTablePath);
        unified_event::logInfo("DHT.PersistenceManager", "Current node count before loading: " + std::to_string(routingTable->getNodeCount()));

        if (!routingTable->loadFromFile(routingTablePath)) {
            unified_event::logError("DHT.PersistenceManager", "Failed to load routing table from " + routingTablePath);
            success = false;
        } else {
            unified_event::logInfo("DHT.PersistenceManager", "Loaded routing table from " + routingTablePath);
            unified_event::logInfo("DHT.PersistenceManager", "Node count after loading: " + std::to_string(routingTable->getNodeCount()));
        }
    } else {
        unified_event::logInfo("DHT.PersistenceManager", "Routing table file does not exist: " + routingTablePath);
    }

    // Get the peer storage path
    std::string peerStoragePath;
    try {
        utility::thread::withLock(m_mutex, [this, &peerStoragePath]() {
            peerStoragePath = m_peerStoragePath;
        }, "PersistenceManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PersistenceManager", e.what());
        // Continue with the routing table success status
        return success;
    }

    // Load peer storage (outside the lock)
    if (fs::exists(peerStoragePath)) {
        try {
            // Open file for binary reading
            std::ifstream file(peerStoragePath, std::ios::binary);
            if (file.is_open()) {
                // Get file size
                file.seekg(0, std::ios::end);
                std::streamsize fileSize = file.tellg();
                file.seekg(0, std::ios::beg);

                if (fileSize == 0) {
                    unified_event::logInfo("DHT.PersistenceManager", "Peer storage file is empty");
                    file.close();
                    return success;
                }

                unified_event::logInfo("DHT.PersistenceManager", "Peer storage file size: " + std::to_string(fileSize) + " bytes");

                // Read file format version
                uint32_t version;
                if (!file.read(reinterpret_cast<char*>(&version), sizeof(version))) {
                    unified_event::logError("DHT.PersistenceManager", "Failed to read version from peer storage file");
                    file.close();
                    return success;
                }

                if (version != 1) {
                    unified_event::logError("DHT.PersistenceManager", "Unsupported peer storage file version: " + std::to_string(version));
                    file.close();
                    return success;
                }

                // Read number of info hashes
                uint32_t numInfoHashes;
                if (!file.read(reinterpret_cast<char*>(&numInfoHashes), sizeof(numInfoHashes))) {
                    unified_event::logError("DHT.PersistenceManager", "Failed to read number of info hashes from peer storage file");
                    file.close();
                    return success;
                }

                unified_event::logInfo("DHT.PersistenceManager", "Loading " + std::to_string(numInfoHashes) + " info hashes from peer storage file");

                size_t totalPeerCount = 0;

                // For each info hash, read its peers
                for (uint32_t i = 0; i < numInfoHashes; i++) {
                    // Read the info hash (20 bytes)
                    InfoHash infoHash;
                    if (!file.read(reinterpret_cast<char*>(infoHash.data()), static_cast<std::streamsize>(infoHash.size()))) {
                        unified_event::logError("DHT.PersistenceManager", "Failed to read info hash from peer storage file");
                        break;
                    }

                    // Read number of peers for this info hash
                    uint32_t numPeers;
                    if (!file.read(reinterpret_cast<char*>(&numPeers), sizeof(numPeers))) {
                        unified_event::logError("DHT.PersistenceManager", "Failed to read number of peers from peer storage file");
                        break;
                    }

                    unified_event::logDebug("DHT.PersistenceManager", "Loading " + std::to_string(numPeers) + " peers for info hash");

                    // For each peer, read its address and port
                    for (uint32_t j = 0; j < numPeers; j++) {
                        // Read IP address length
                        uint16_t ipLength;
                        if (!file.read(reinterpret_cast<char*>(&ipLength), sizeof(ipLength))) {
                            unified_event::logError("DHT.PersistenceManager", "Failed to read IP address length from peer storage file");
                            break;
                        }

                        // Read IP address
                        std::string ipAddress(ipLength, '\0');
                        if (!file.read(&ipAddress[0], static_cast<std::streamsize>(ipLength))) {
                            unified_event::logError("DHT.PersistenceManager", "Failed to read IP address from peer storage file");
                            break;
                        }

                        // Read port
                        uint16_t port;
                        if (!file.read(reinterpret_cast<char*>(&port), sizeof(port))) {
                            unified_event::logError("DHT.PersistenceManager", "Failed to read port from peer storage file");
                            break;
                        }

                        // Create endpoint and add peer
                        network::NetworkAddress networkAddress(ipAddress);
                        network::EndPoint endpoint(networkAddress, port);

                        peerStorage->addPeer(infoHash, endpoint);
                        totalPeerCount++;
                    }
                }

                file.close();

                unified_event::logInfo("DHT.PersistenceManager", "Successfully loaded peer storage from " + peerStoragePath +
                                      " with " + std::to_string(numInfoHashes) + " info hashes and " +
                                      std::to_string(totalPeerCount) + " peers");
            } else {
                unified_event::logError("DHT.PersistenceManager", "Failed to open file for reading: " + peerStoragePath);
                success = false;
            }
        } catch (const std::exception& e) {
            unified_event::logError("DHT.PersistenceManager", "Exception while loading peer storage: " + std::string(e.what()));
            success = false;
        }
    } else {
        unified_event::logDebug("DHT.PersistenceManager", "Peer storage file does not exist: " + peerStoragePath);
    }

    return success;
}

void PersistenceManager::savePeriodically() {
    unified_event::logDebug("DHT.PersistenceManager", "Starting periodic save thread");

    while (m_running) {
        // Sleep for the save interval
        unified_event::logTrace("DHT.PersistenceManager", "Sleeping for " + std::to_string(m_saveInterval.count()) + " minutes");
        std::this_thread::sleep_for(m_saveInterval);

        // Check if we're still running after the sleep
        if (!m_running) {
            unified_event::logDebug("DHT.PersistenceManager", "Thread woke up but manager is no longer running, exiting thread");
            break;
        }

        // Save data
        unified_event::logDebug("DHT.PersistenceManager", "Performing periodic save");
        bool saveResult = saveNow();
        unified_event::logDebug("DHT.PersistenceManager", "Periodic save completed with result: " + std::string(saveResult ? "success" : "failure"));
    }

    unified_event::logDebug("DHT.PersistenceManager", "Periodic save thread stopped");
}

bool PersistenceManager::createConfigDir() {
    try {
        if (!fs::exists(m_configDir)) {
            unified_event::logDebug("DHT.PersistenceManager", "Creating config directory: " + m_configDir);
            if (!fs::create_directories(m_configDir)) {
                unified_event::logError("DHT.PersistenceManager", "Failed to create config directory: " + m_configDir);
                return false;
            }
        }
        return true;
    } catch (const std::exception& e) {
        unified_event::logError("DHT.PersistenceManager", "Exception while creating config directory: " + std::string(e.what()));
        return false;
    }
}

} // namespace dht_hunter::dht
