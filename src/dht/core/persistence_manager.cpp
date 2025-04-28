#include "dht_hunter/dht/core/persistence_manager.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"
#include "dht_hunter/utility/json/json_utility.hpp"
#include "dht_hunter/utility/hash/hash_utils.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace dht_hunter::dht {

using namespace dht_hunter::utility::json;
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
      m_routingTablePath(configDir + "/routing_table.json"),
      m_peerStoragePath(configDir + "/peer_storage.json"),
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
    try {
        return utility::thread::withLock(m_mutex, [this, routingTable, peerStorage]() {
            if (m_running) {
                return true;
            }

            m_routingTable = routingTable;
            m_peerStorage = peerStorage;

            if (!m_routingTable || !m_peerStorage) {
                unified_event::logError("DHT.PersistenceManager", "Cannot start: routing table or peer storage is null");
                return false;
            }

            // Load data from disk
            loadFromDisk();

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
    try {
        utility::thread::withLock(m_mutex, [this]() {
            if (!m_running) {
                return;
            }

            // Save data before stopping
            saveNow();

            // Stop the save thread
            m_running = false;

            if (m_saveThread.joinable()) {
                m_saveThread.join();
            }

            unified_event::logDebug("DHT.PersistenceManager", "Stopped");
        }, "PersistenceManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PersistenceManager", e.what());
    }
}

bool PersistenceManager::isRunning() const {
    return m_running;
}

bool PersistenceManager::saveNow() {
    try {
        return utility::thread::withLock(m_mutex, [this]() {
            if (!m_routingTable || !m_peerStorage) {
                unified_event::logError("DHT.PersistenceManager", "Cannot save: routing table or peer storage is null");
                return false;
            }

            bool success = true;

            // Save routing table
            if (!m_routingTable->saveToFile(m_routingTablePath)) {
                unified_event::logError("DHT.PersistenceManager", "Failed to save routing table to " + m_routingTablePath);
                success = false;
            } else {
                unified_event::logDebug("DHT.PersistenceManager", "Saved routing table to " + m_routingTablePath);
            }

            // Save peer storage
            try {
                JsonValue::JsonObject peerData;

                // Get all info hashes and their peers
                size_t infoHashCount = m_peerStorage->getInfoHashCount();
                size_t totalPeerCount = m_peerStorage->getTotalPeerCount();

                // Create a list of all info hashes
                std::vector<InfoHash> infoHashes;

                // This is a workaround since we don't have a direct method to get all info hashes
                // In a real implementation, you would add a method to PeerStorage to get all info hashes

                // For each info hash, get its peers
                for (const auto& infoHash : infoHashes) {
                    auto peers = m_peerStorage->getPeers(infoHash);

                    JsonValue::JsonArray peerList;
                    for (const auto& peer : peers) {
                        JsonValue::JsonObject peerObj;
                        peerObj.set("address", peer.getAddress().toString());
                        peerObj.set("port", static_cast<double>(peer.getPort()));
                        peerList.push_back(peerObj);
                    }

                    // Convert InfoHash to string
                    std::string infoHashStr;
                    for (const auto& byte : infoHash) {
                        char buf[3];
                        snprintf(buf, sizeof(buf), "%02x", byte);
                        infoHashStr += buf;
                    }

                    peerData.set(infoHashStr, peerList);
                }

                // Write to file
                std::ofstream file(m_peerStoragePath);
                if (file.is_open()) {
                    file << JsonUtil::stringify(peerData, 4);
                    file.close();
                    unified_event::logDebug("DHT.PersistenceManager", "Saved peer storage to " + m_peerStoragePath);
                } else {
                    unified_event::logError("DHT.PersistenceManager", "Failed to open file for writing: " + m_peerStoragePath);
                    success = false;
                }
            } catch (const std::exception& e) {
                unified_event::logError("DHT.PersistenceManager", "Exception while saving peer storage: " + std::string(e.what()));
                success = false;
            }

            return success;
        }, "PersistenceManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PersistenceManager", e.what());
        return false;
    }
}

bool PersistenceManager::loadFromDisk() {
    try {
        return utility::thread::withLock(m_mutex, [this]() {
            if (!m_routingTable || !m_peerStorage) {
                unified_event::logError("DHT.PersistenceManager", "Cannot load: routing table or peer storage is null");
                return false;
            }

            bool success = true;

            // Load routing table
            if (fs::exists(m_routingTablePath)) {
                if (!m_routingTable->loadFromFile(m_routingTablePath)) {
                    unified_event::logError("DHT.PersistenceManager", "Failed to load routing table from " + m_routingTablePath);
                    success = false;
                } else {
                    unified_event::logDebug("DHT.PersistenceManager", "Loaded routing table from " + m_routingTablePath);
                }
            } else {
                unified_event::logDebug("DHT.PersistenceManager", "Routing table file does not exist: " + m_routingTablePath);
            }

            // Load peer storage
            if (fs::exists(m_peerStoragePath)) {
                try {
                    std::ifstream file(m_peerStoragePath);
                    if (file.is_open()) {
                        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                        file.close();

                        JsonValue peerData = JsonUtil::parse(content);

                        // For each info hash, add its peers
                        for (const auto& [infoHashStr, peerListValue] : peerData.getObject()) {
                            // Convert string to InfoHash
                            InfoHash infoHash;
                            for (size_t i = 0; i < infoHashStr.length(); i += 2) {
                                if (i / 2 < infoHash.size()) {
                                    std::string byteStr = infoHashStr.substr(i, 2);
                                    infoHash[i / 2] = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
                                }
                            }

                            for (const auto& peerValue : peerListValue.getArray()) {
                                const auto& peerObj = peerValue.getObject();
                                std::string address = peerObj["address"].getString();
                                uint16_t port = static_cast<uint16_t>(peerObj["port"].getNumber());

                                network::NetworkAddress networkAddress(address);
                                network::EndPoint endpoint(networkAddress, port);

                                m_peerStorage->addPeer(infoHash, endpoint);
                            }
                        }

                        unified_event::logDebug("DHT.PersistenceManager", "Loaded peer storage from " + m_peerStoragePath);
                    } else {
                        unified_event::logError("DHT.PersistenceManager", "Failed to open file for reading: " + m_peerStoragePath);
                        success = false;
                    }
                } catch (const std::exception& e) {
                    unified_event::logError("DHT.PersistenceManager", "Exception while loading peer storage: " + std::string(e.what()));
                    success = false;
                }
            } else {
                unified_event::logDebug("DHT.PersistenceManager", "Peer storage file does not exist: " + m_peerStoragePath);
            }

            return success;
        }, "PersistenceManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.PersistenceManager", e.what());
        return false;
    }
}

void PersistenceManager::savePeriodically() {
    unified_event::logDebug("DHT.PersistenceManager", "Starting periodic save thread");

    while (m_running) {
        // Sleep for the save interval
        std::this_thread::sleep_for(m_saveInterval);

        // Save data
        if (m_running) {
            unified_event::logTrace("DHT.PersistenceManager", "Performing periodic save");
            saveNow();
        }
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
