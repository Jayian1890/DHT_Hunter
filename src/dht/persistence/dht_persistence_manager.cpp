#include "dht_hunter/dht/persistence/dht_persistence_manager.hpp"
#include "dht_hunter/dht/routing/dht_routing_manager.hpp"
#include "dht_hunter/dht/storage/dht_peer_storage.hpp"
#include "dht_hunter/dht/transactions/dht_transaction_manager.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include <filesystem>
#include <fstream>

DEFINE_COMPONENT_LOGGER("DHT", "PersistenceManager")

namespace dht_hunter::dht {

DHTPersistenceManager::DHTPersistenceManager(const DHTNodeConfig& config,
                                           std::shared_ptr<DHTRoutingManager> routingManager,
                                           std::shared_ptr<DHTPeerStorage> peerStorage,
                                           std::shared_ptr<DHTTransactionManager> transactionManager)
    : m_config(config),
      m_routingManager(routingManager),
      m_peerStorage(peerStorage),
      m_transactionManager(transactionManager),
      m_running(false) {
    getLogger()->info("Persistence manager created");
}

DHTPersistenceManager::~DHTPersistenceManager() {
    stop();
}

bool DHTPersistenceManager::start() {
    if (m_running) {
        getLogger()->warning("Persistence manager already running");
        return false;
    }

    // Load all data
    loadAll();

    // Start the save thread
    m_running = true;
    m_saveThread = std::thread(&DHTPersistenceManager::savePeriodically, this);

    getLogger()->info("Persistence manager started");
    return true;
}

void DHTPersistenceManager::stop() {
    // Use atomic operation to prevent multiple stops
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false)) {
        // Already stopped or stopping
        return;
    }

    getLogger()->info("Stopping persistence manager");

    // Save all data before stopping
    saveAll();

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

    getLogger()->info("Persistence manager stopped");
}

bool DHTPersistenceManager::isRunning() const {
    return m_running;
}

bool DHTPersistenceManager::saveAll() {
    std::lock_guard<util::CheckedMutex> lock(m_saveMutex);

    bool success = true;

    // Save the routing table
    if (m_routingManager) {
        if (!saveRoutingTable(m_config.getRoutingTablePath())) {
            success = false;
        }
    }

    // Save the peer cache
    if (m_peerStorage) {
        if (!savePeerCache(m_config.getPeerCachePath())) {
            success = false;
        }
    }

    // Save the transactions
    if (m_transactionManager && m_config.getSaveTransactionsOnShutdown()) {
        if (!saveTransactions(m_config.getTransactionsPath())) {
            success = false;
        }
    }

    return success;
}

bool DHTPersistenceManager::loadAll() {
    bool success = true;

    // Load the routing table
    if (m_routingManager) {
        if (!loadRoutingTable(m_config.getRoutingTablePath())) {
            success = false;
        }
    }

    // Load the peer cache
    if (m_peerStorage) {
        if (!loadPeerCache(m_config.getPeerCachePath())) {
            success = false;
        }
    }

    // Load the transactions
    if (m_transactionManager && m_config.getLoadTransactionsOnStartup()) {
        if (!loadTransactions(m_config.getTransactionsPath())) {
            success = false;
        }
    }

    return success;
}

bool DHTPersistenceManager::saveRoutingTable(const std::string& filePath) {
    if (!m_routingManager) {
        getLogger()->error("Cannot save routing table: Routing manager not available");
        return false;
    }

    getLogger()->debug("Saving routing table to {}", filePath);
    return m_routingManager->saveRoutingTable(filePath);
}

bool DHTPersistenceManager::loadRoutingTable(const std::string& filePath) {
    if (!m_routingManager) {
        getLogger()->error("Cannot load routing table: Routing manager not available");
        return false;
    }

    if (!std::filesystem::exists(filePath)) {
        getLogger()->info("Routing table file does not exist: {}", filePath);
        return false;
    }

    getLogger()->debug("Loading routing table from {}", filePath);
    return m_routingManager->loadRoutingTable(filePath);
}

bool DHTPersistenceManager::savePeerCache(const std::string& filePath) {
    if (!m_peerStorage) {
        getLogger()->error("Cannot save peer cache: Peer storage not available");
        return false;
    }

    getLogger()->debug("Saving peer cache to {}", filePath);
    return m_peerStorage->savePeerCache(filePath);
}

bool DHTPersistenceManager::loadPeerCache(const std::string& filePath) {
    if (!m_peerStorage) {
        getLogger()->error("Cannot load peer cache: Peer storage not available");
        return false;
    }

    if (!std::filesystem::exists(filePath)) {
        getLogger()->info("Peer cache file does not exist: {}", filePath);
        return false;
    }

    getLogger()->debug("Loading peer cache from {}", filePath);
    return m_peerStorage->loadPeerCache(filePath);
}

bool DHTPersistenceManager::saveTransactions(const std::string& filePath) {
    if (!m_transactionManager) {
        getLogger()->error("Cannot save transactions: Transaction manager not available");
        return false;
    }

    getLogger()->debug("Saving transactions to {}", filePath);
    return m_transactionManager->saveTransactions(filePath);
}

bool DHTPersistenceManager::loadTransactions(const std::string& filePath) {
    if (!m_transactionManager) {
        getLogger()->error("Cannot load transactions: Transaction manager not available");
        return false;
    }

    if (!std::filesystem::exists(filePath)) {
        getLogger()->info("Transactions file does not exist: {}", filePath);
        return false;
    }

    getLogger()->debug("Loading transactions from {}", filePath);
    return m_transactionManager->loadTransactions(filePath);
}

bool DHTPersistenceManager::saveNodeID(const std::string& filePath, const std::array<uint8_t, 20>& nodeID) {
    // Create the directory if it doesn't exist
    std::filesystem::path path(filePath);
    std::filesystem::create_directories(path.parent_path());

    // Save the node ID
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        getLogger()->error("Failed to open node ID file for writing: {}", filePath);
        return false;
    }

    file.write(reinterpret_cast<const char*>(nodeID.data()), static_cast<std::streamsize>(nodeID.size()));
    if (!file) {
        getLogger()->error("Failed to write node ID to file: {}", filePath);
        return false;
    }

    getLogger()->info("Node ID saved to {}", filePath);
    return true;
}

bool DHTPersistenceManager::loadNodeID(const std::string& filePath, std::array<uint8_t, 20>& nodeID) {
    // Check if the file exists
    if (!std::filesystem::exists(filePath)) {
        getLogger()->error("Node ID file does not exist: {}", filePath);
        return false;
    }

    // Load the node ID
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        getLogger()->error("Failed to open node ID file for reading: {}", filePath);
        return false;
    }

    if (!file.read(reinterpret_cast<char*>(nodeID.data()), static_cast<std::streamsize>(nodeID.size()))) {
        getLogger()->error("Failed to read node ID from file: {}", filePath);
        return false;
    }

    getLogger()->info("Node ID loaded from {}", filePath);
    return true;
}

void DHTPersistenceManager::savePeriodically() {
    getLogger()->debug("Starting periodic save thread");

    while (m_running) {
        // Sleep for the save interval
        std::this_thread::sleep_for(std::chrono::seconds(ROUTING_TABLE_SAVE_INTERVAL));

        // Check if we should still be running
        if (!m_running) {
            break;
        }

        // Save all data
        std::lock_guard<util::CheckedMutex> lock(m_saveMutex);
        
        // Save the routing table
        if (m_routingManager) {
            saveRoutingTable(m_config.getRoutingTablePath());
        }
        
        // Save the peer cache
        if (m_peerStorage) {
            savePeerCache(m_config.getPeerCachePath());
        }
        
        // Save the transactions
        if (m_transactionManager && m_config.getSaveTransactionsOnShutdown()) {
            saveTransactions(m_config.getTransactionsPath());
        }
    }

    getLogger()->debug("Periodic save thread exiting");
}

} // namespace dht_hunter::dht
