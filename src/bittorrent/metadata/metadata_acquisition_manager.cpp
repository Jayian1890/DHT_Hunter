#include "dht_hunter/bittorrent/metadata/metadata_acquisition_manager.hpp"
#include "dht_hunter/utility/metadata/metadata_utils.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"

namespace dht_hunter::bittorrent::metadata {

// Initialize static members
std::shared_ptr<MetadataAcquisitionManager> MetadataAcquisitionManager::s_instance = nullptr;
std::mutex MetadataAcquisitionManager::s_instanceMutex;

std::shared_ptr<MetadataAcquisitionManager> MetadataAcquisitionManager::getInstance(
    std::shared_ptr<dht::PeerStorage> peerStorage) {

    try {
        return utility::thread::withLock(s_instanceMutex, [&peerStorage]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<MetadataAcquisitionManager>(new MetadataAcquisitionManager(peerStorage));
            } else if (peerStorage && !s_instance->m_peerStorage) {
                // Update the peer storage if it wasn't set before
                s_instance->m_peerStorage = peerStorage;
            }
            return s_instance;
        }, "MetadataAcquisitionManager::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("BitTorrent.MetadataAcquisitionManager", e.what());
        return nullptr;
    }
}

MetadataAcquisitionManager::MetadataAcquisitionManager(std::shared_ptr<dht::PeerStorage> peerStorage)
    : m_peerStorage(peerStorage),
      m_metadataExchange(std::make_shared<MetadataExchange>()),
      m_eventBus(unified_event::EventBus::getInstance()),
      m_processingIntervalSeconds(5),
      m_maxConcurrentAcquisitions(5),
      m_acquisitionTimeoutSeconds(60),
      m_maxRetryCount(3),
      m_retryDelayBaseSeconds(300) {

    // Load configuration
    loadConfiguration();
}

MetadataAcquisitionManager::~MetadataAcquisitionManager() {
    stop();

    // Clear the singleton instance
    try {
        utility::thread::withLock(s_instanceMutex, [this]() {
            if (s_instance.get() == this) {
                s_instance.reset();
            }
        }, "MetadataAcquisitionManager::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("BitTorrent.MetadataAcquisitionManager", e.what());
    }
}

bool MetadataAcquisitionManager::start() {
    // Check if already running
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        // Already running
        return true;
    }

    // Subscribe to InfoHashDiscoveredEvent
    m_infoHashDiscoveredSubscription = m_eventBus->subscribe(
        unified_event::EventType::Custom,
        [this](const std::shared_ptr<unified_event::Event>& event) {
            if (event->getName() == "InfoHashDiscovered") {
                auto infoHashEvent = std::dynamic_pointer_cast<unified_event::InfoHashDiscoveredEvent>(event);
                if (infoHashEvent) {
                    unified_event::logDebug("BitTorrent.MetadataAcquisitionManager", "Received InfoHashDiscoveredEvent for " +
                                          types::infoHashToString(infoHashEvent->getInfoHash()));
                    handleInfoHashDiscoveredEvent(infoHashEvent);
                }
            }
        });

    // Start the processing thread
    m_processingThread = std::thread(&MetadataAcquisitionManager::processAcquisitionQueuePeriodically, this);

    // Publish a system started event
    auto startedEvent = std::make_shared<unified_event::SystemStartedEvent>("BitTorrent.MetadataAcquisitionManager");
    m_eventBus->publish(startedEvent);

    unified_event::logInfo("BitTorrent.MetadataAcquisitionManager", "Started metadata acquisition manager");
    return true;
}

void MetadataAcquisitionManager::stop() {
    // Check if already stopped
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
        // Already stopped
        return;
    }

    // Unsubscribe from events
    if (m_infoHashDiscoveredSubscription > 0) {
        m_eventBus->unsubscribe(m_infoHashDiscoveredSubscription);
        m_infoHashDiscoveredSubscription = 0;
    }

    // Stop the processing thread
    m_processingCondition.notify_all();
    if (m_processingThread.joinable()) {
        m_processingThread.join();
    }

    // Publish a system stopped event
    auto stoppedEvent = std::make_shared<unified_event::SystemStoppedEvent>("BitTorrent.MetadataAcquisitionManager");
    m_eventBus->publish(stoppedEvent);

    unified_event::logInfo("BitTorrent.MetadataAcquisitionManager", "Stopped metadata acquisition manager");
}

bool MetadataAcquisitionManager::isRunning() const {
    return m_running.load(std::memory_order_acquire);
}

bool MetadataAcquisitionManager::acquireMetadata(const types::InfoHash& infoHash, int priority) {
    unified_event::logInfo("BitTorrent.MetadataAcquisitionManager", "Attempting to acquire metadata for info hash: " + types::infoHashToString(infoHash) +
                         " with priority: " + std::to_string(priority));

    // Check if we already have metadata for this info hash
    auto metadata = utility::metadata::MetadataUtils::getInfoHashMetadata(infoHash);
    if (metadata && !metadata->getName().empty() && metadata->getTotalSize() > 0) {
        unified_event::logInfo("BitTorrent.MetadataAcquisitionManager", "Already have metadata for info hash: " + types::infoHashToString(infoHash));
        return true;
    }

    // Check if acquisition is already in progress
    if (isAcquisitionInProgress(infoHash)) {
        // Update the priority if the new priority is higher
        try {
            utility::thread::withLock(m_activeAcquisitionsMutex, [this, &infoHash, priority]() {
                auto it = m_activeAcquisitions.find(infoHash);
                if (it != m_activeAcquisitions.end() && priority > it->second->priority) {
                    it->second->priority = priority;
                    unified_event::logInfo("BitTorrent.MetadataAcquisitionManager", "Updated priority for info hash: " +
                                         types::infoHashToString(infoHash) +
                                         " to " + std::to_string(priority));
                }
            }, "MetadataAcquisitionManager::m_activeAcquisitionsMutex");
        } catch (const utility::thread::LockTimeoutException& e) {
            unified_event::logError("BitTorrent.MetadataAcquisitionManager", e.what());
        }

        unified_event::logInfo("BitTorrent.MetadataAcquisitionManager", "Metadata acquisition already in progress for info hash: " + types::infoHashToString(infoHash));
        return true;
    }

    // Check if this is a retry of a failed acquisition
    bool isRetry = false;
    try {
        utility::thread::withLock(m_failedAcquisitionsMutex, [this, &infoHash, &isRetry]() {
            auto it = m_failedAcquisitions.find(infoHash);
            if (it != m_failedAcquisitions.end()) {
                isRetry = true;
                m_failedAcquisitions.erase(it);
            }
        }, "MetadataAcquisitionManager::m_failedAcquisitionsMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("BitTorrent.MetadataAcquisitionManager", e.what());
    }

    // Create a new acquisition task
    auto task = std::make_shared<AcquisitionTask>(infoHash, priority);
    if (isRetry) {
        task->status = "Retry";
        task->retryCount++;
        m_retryAttempts++;
    }

    // Add to the acquisition queue
    try {
        utility::thread::withLock(m_queueMutex, [this, &infoHash, &task]() {
            m_acquisitionQueue.insert(infoHash);

            // Add to active acquisitions
            utility::thread::withLock(m_activeAcquisitionsMutex, [this, &infoHash, &task]() {
                m_activeAcquisitions[infoHash] = task;
            }, "MetadataAcquisitionManager::m_activeAcquisitionsMutex");

            unified_event::logInfo("BitTorrent.MetadataAcquisitionManager", "Added info hash to acquisition queue: " +
                                 types::infoHashToString(infoHash) +
                                 ", queue size: " +
                                 std::to_string(m_acquisitionQueue.size()));
        }, "MetadataAcquisitionManager::m_queueMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("BitTorrent.MetadataAcquisitionManager", e.what());
        return false;
    }

    // Increment total attempts counter
    m_totalAttempts++;

    // Notify the processing thread
    m_processingCondition.notify_one();
    unified_event::logInfo("BitTorrent.MetadataAcquisitionManager", "Notified processing thread for info hash: " + types::infoHashToString(infoHash));
    return true;
}

bool MetadataAcquisitionManager::isAcquisitionInProgress(const types::InfoHash& infoHash) const {
    try {
        return utility::thread::withLock(m_activeAcquisitionsMutex, [this, &infoHash]() {
            return m_activeAcquisitions.find(infoHash) != m_activeAcquisitions.end();
        }, "MetadataAcquisitionManager::m_activeAcquisitionsMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("BitTorrent.MetadataAcquisitionManager", e.what());
        return false;
    }
}

std::string MetadataAcquisitionManager::getAcquisitionStatus(const types::InfoHash& infoHash) const {
    try {
        return utility::thread::withLock(m_activeAcquisitionsMutex, [this, &infoHash]() {
            auto it = m_activeAcquisitions.find(infoHash);
            if (it != m_activeAcquisitions.end()) {
                return it->second->status;
            }
            return std::string("");
        }, "MetadataAcquisitionManager::m_activeAcquisitionsMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("BitTorrent.MetadataAcquisitionManager", e.what());
        return "Error";
    }
}

std::unordered_map<std::string, int> MetadataAcquisitionManager::getStatistics() const {
    std::unordered_map<std::string, int> stats;

    // Add counters
    stats["totalAttempts"] = m_totalAttempts.load();
    stats["successfulAcquisitions"] = m_successfulAcquisitionsCount.load();
    stats["failedAcquisitions"] = m_failedAcquisitionsCount.load();
    stats["retryAttempts"] = m_retryAttempts.load();

    // Add queue sizes
    try {
        utility::thread::withLock(m_queueMutex, [this, &stats]() {
            stats["queueSize"] = static_cast<int>(m_acquisitionQueue.size());
        }, "MetadataAcquisitionManager::m_queueMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("BitTorrent.MetadataAcquisitionManager", e.what());
        stats["queueSize"] = -1;
    }

    try {
        utility::thread::withLock(m_activeAcquisitionsMutex, [this, &stats]() {
            stats["activeAcquisitions"] = static_cast<int>(m_activeAcquisitions.size());
        }, "MetadataAcquisitionManager::m_activeAcquisitionsMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("BitTorrent.MetadataAcquisitionManager", e.what());
        stats["activeAcquisitions"] = -1;
    }

    try {
        utility::thread::withLock(m_failedAcquisitionsMutex, [this, &stats]() {
            stats["pendingRetries"] = static_cast<int>(m_failedAcquisitions.size());
        }, "MetadataAcquisitionManager::m_failedAcquisitionsMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("BitTorrent.MetadataAcquisitionManager", e.what());
        stats["pendingRetries"] = -1;
    }

    return stats;
}

size_t MetadataAcquisitionManager::getActiveAcquisitionCount() const {
    try {
        return utility::thread::withLock(m_activeAcquisitionsMutex, [this]() {
            return m_activeAcquisitions.size();
        }, "MetadataAcquisitionManager::m_activeAcquisitionsMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("BitTorrent.MetadataAcquisitionManager", e.what());
        return 0;
    }
}

void MetadataAcquisitionManager::processAcquisitionQueue() {
    unified_event::logDebug("BitTorrent.MetadataAcquisitionManager", "Processing acquisition queue");

    // Process failed acquisitions first
    processFailedAcquisitions();

    // Clean up timed out acquisitions
    try {
        utility::thread::withLock(m_activeAcquisitionsMutex, [this]() {
            auto now = std::chrono::steady_clock::now();
            std::vector<types::InfoHash> timedOutInfoHashes;

            for (auto it = m_activeAcquisitions.begin(); it != m_activeAcquisitions.end();) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second->startTime).count();

                if (elapsed > m_acquisitionTimeoutSeconds) {
                    unified_event::logWarning("BitTorrent.MetadataAcquisitionManager", "Metadata acquisition timed out for info hash: " +
                                             types::infoHashToString(it->first));

                    // Add to timed out list
                    timedOutInfoHashes.push_back(it->first);

                    // Move to failed acquisitions if retry count is below max
                    if (it->second->retryCount < m_maxRetryCount) {
                        handleFailedAcquisition(it->first, "Timed out");
                    } else {
                        unified_event::logWarning("BitTorrent.MetadataAcquisitionManager", "Maximum retry count reached for info hash: " +
                                                types::infoHashToString(it->first));
                    }

                    it = m_activeAcquisitions.erase(it);
                } else {
                    // Update status to show elapsed time
                    it->second->status = "In Progress " + std::to_string(elapsed) + "s";
                    ++it;
                }
            }

            unified_event::logDebug("BitTorrent.MetadataAcquisitionManager", "Active acquisitions: " + std::to_string(m_activeAcquisitions.size()));
        }, "MetadataAcquisitionManager::m_activeAcquisitionsMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("BitTorrent.MetadataAcquisitionManager", e.what());
    }

    // Process the queue if we have capacity
    size_t activeCount = getActiveAcquisitionCount();
    if (activeCount >= static_cast<size_t>(m_maxConcurrentAcquisitions)) {
        unified_event::logDebug("BitTorrent.MetadataAcquisitionManager", "Maximum concurrent acquisitions reached: " +
                              std::to_string(activeCount) +
                              "/" +
                              std::to_string(m_maxConcurrentAcquisitions));
        return;
    }

    // Get the next info hash from the queue
    types::InfoHash infoHash;
    bool hasNext = false;

    try {
        utility::thread::withLock(m_queueMutex, [this, &infoHash, &hasNext]() {
            if (!m_acquisitionQueue.empty()) {
                auto it = m_acquisitionQueue.begin();
                infoHash = *it;
                m_acquisitionQueue.erase(it);
                hasNext = true;

                unified_event::logInfo("BitTorrent.MetadataAcquisitionManager", "Dequeued info hash for metadata acquisition: " +
                                     types::infoHashToString(infoHash) +
                                     ", queue size: " +
                                     std::to_string(m_acquisitionQueue.size()));
            } else {
                unified_event::logDebug("BitTorrent.MetadataAcquisitionManager", "Acquisition queue is empty");
            }
        }, "MetadataAcquisitionManager::m_queueMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("BitTorrent.MetadataAcquisitionManager", e.what());
        return;
    }

    if (!hasNext) {
        return;
    }

    // Check if we have peers for this info hash
    if (!m_peerStorage) {
        unified_event::logWarning("BitTorrent.MetadataAcquisitionManager", "Peer storage not available");
        return;
    }

    std::vector<network::EndPoint> peers = m_peerStorage->getPeers(infoHash);
    if (peers.empty()) {
        unified_event::logWarning("BitTorrent.MetadataAcquisitionManager", "No peers available for info hash: " + types::infoHashToString(infoHash));
        return;
    }

    // Update the task status
    try {
        utility::thread::withLock(m_activeAcquisitionsMutex, [this, &infoHash]() {
            auto it = m_activeAcquisitions.find(infoHash);
            if (it != m_activeAcquisitions.end()) {
                it->second->status = "In Progress";
            }
        }, "MetadataAcquisitionManager::m_activeAcquisitionsMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("BitTorrent.MetadataAcquisitionManager", e.what());
        return;
    }

    // Start the acquisition
    m_metadataExchange->acquireMetadata(infoHash, peers, [this, infoHash](bool success) {
        if (success) {
            handleSuccessfulAcquisition(infoHash);
        } else {
            handleFailedAcquisition(infoHash, "Failed to acquire metadata");
        }

        // Notify the processing thread to continue
        m_processingCondition.notify_one();
    }, 3); // Try up to 3 peers concurrently
}

void MetadataAcquisitionManager::loadConfiguration() {
    auto configManager = utility::config::ConfigurationManager::getInstance();
    if (!configManager) {
        unified_event::logWarning("BitTorrent.MetadataAcquisitionManager", "Configuration manager is null, using default values");
        return;
    }

    // Load values from configuration
    m_processingIntervalSeconds = configManager->getInt("metadata.processingInterval", 5);
    m_maxConcurrentAcquisitions = configManager->getInt("metadata.maxConcurrentAcquisitions", 5);
    m_acquisitionTimeoutSeconds = configManager->getInt("metadata.acquisitionTimeout", 60);
    m_maxRetryCount = configManager->getInt("metadata.maxRetryCount", 3);
    m_retryDelayBaseSeconds = configManager->getInt("metadata.retryDelayBase", 300);

    unified_event::logInfo("BitTorrent.MetadataAcquisitionManager", "Loaded configuration: processingInterval=" + std::to_string(m_processingIntervalSeconds) +
                          ", maxConcurrentAcquisitions=" + std::to_string(m_maxConcurrentAcquisitions) +
                          ", acquisitionTimeout=" + std::to_string(m_acquisitionTimeoutSeconds) +
                          ", maxRetryCount=" + std::to_string(m_maxRetryCount) +
                          ", retryDelayBase=" + std::to_string(m_retryDelayBaseSeconds));
}

void MetadataAcquisitionManager::processAcquisitionQueuePeriodically() {
    while (m_running) {
        // Wait for the processing interval or until we're notified
        std::unique_lock<std::mutex> lock(m_processingMutex);
        m_processingCondition.wait_for(lock, std::chrono::seconds(m_processingIntervalSeconds), [this]() {
            return !m_running || !m_acquisitionQueue.empty();
        });

        if (!m_running) {
            break;
        }

        // Process the queue
        processAcquisitionQueue();
    }
}

void MetadataAcquisitionManager::handleInfoHashDiscoveredEvent(const std::shared_ptr<unified_event::InfoHashDiscoveredEvent>& event) {
    if (!event) {
        unified_event::logWarning("BitTorrent.MetadataAcquisitionManager", "Received null InfoHashDiscoveredEvent");
        return;
    }

    unified_event::logInfo("BitTorrent.MetadataAcquisitionManager", "Processing InfoHashDiscoveredEvent for " +
                         types::infoHashToString(event->getInfoHash()));

    // Automatically acquire metadata for newly discovered info hashes
    bool result = acquireMetadata(event->getInfoHash());

    std::string resultStr = result ? "started" : "failed";
    unified_event::logInfo("BitTorrent.MetadataAcquisitionManager", "Metadata acquisition " + resultStr + " for " + types::infoHashToString(event->getInfoHash()));
}

void MetadataAcquisitionManager::processFailedAcquisitions() {
    std::vector<types::InfoHash> readyForRetry;

    try {
        utility::thread::withLock(m_failedAcquisitionsMutex, [this, &readyForRetry]() {
            auto now = std::chrono::steady_clock::now();

            for (auto it = m_failedAcquisitions.begin(); it != m_failedAcquisitions.end();) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second->lastRetryTime).count();
                int delaySeconds = m_retryDelayBaseSeconds * (1 << it->second->retryCount); // Exponential backoff

                if (elapsed >= delaySeconds) {
                    // Ready for retry
                    readyForRetry.push_back(it->first);
                    it = m_failedAcquisitions.erase(it);
                } else {
                    ++it;
                }
            }
        }, "MetadataAcquisitionManager::m_failedAcquisitionsMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("BitTorrent.MetadataAcquisitionManager", e.what());
        return;
    }

    // Retry acquisitions
    for (const auto& infoHash : readyForRetry) {
        unified_event::logInfo("BitTorrent.MetadataAcquisitionManager", "Retrying metadata acquisition for info hash: " + types::infoHashToString(infoHash));
        acquireMetadata(infoHash);
    }
}

void MetadataAcquisitionManager::handleSuccessfulAcquisition(const types::InfoHash& infoHash) {
    unified_event::logInfo("BitTorrent.MetadataAcquisitionManager", "Successfully acquired metadata for info hash: " + types::infoHashToString(infoHash));

    // Remove from active acquisitions
    try {
        utility::thread::withLock(m_activeAcquisitionsMutex, [this, &infoHash]() {
            m_activeAcquisitions.erase(infoHash);
        }, "MetadataAcquisitionManager::m_activeAcquisitionsMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("BitTorrent.MetadataAcquisitionManager", e.what());
    }

    // Increment successful acquisitions counter
    m_successfulAcquisitionsCount++;

    // Publish a metadata acquired event
    auto metadata = utility::metadata::MetadataUtils::getInfoHashMetadata(infoHash);
    if (metadata) {
        auto event = std::make_shared<unified_event::InfoHashMetadataAcquiredEvent>(
            "BitTorrent.MetadataAcquisitionManager",
            infoHash,
            metadata->getName(),
            metadata->getTotalSize(),
            metadata->getFiles().size()
        );
        m_eventBus->publish(event);
    }
}

void MetadataAcquisitionManager::handleFailedAcquisition(const types::InfoHash& infoHash, const std::string& error) {
    unified_event::logWarning("BitTorrent.MetadataAcquisitionManager", "Failed to acquire metadata for info hash: " +
                            types::infoHashToString(infoHash) +
                            ", error: " +
                            error);

    // Get the task from active acquisitions
    std::shared_ptr<AcquisitionTask> task;
    try {
        utility::thread::withLock(m_activeAcquisitionsMutex, [this, &infoHash, &task]() {
            auto it = m_activeAcquisitions.find(infoHash);
            if (it != m_activeAcquisitions.end()) {
                task = it->second;
                m_activeAcquisitions.erase(it);
            }
        }, "MetadataAcquisitionManager::m_activeAcquisitionsMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("BitTorrent.MetadataAcquisitionManager", e.what());
        return;
    }

    if (!task) {
        return;
    }

    // Increment failed acquisitions counter
    m_failedAcquisitionsCount++;

    // Check if we should retry
    if (task->retryCount < m_maxRetryCount) {
        // Add to failed acquisitions for retry
        try {
            utility::thread::withLock(m_failedAcquisitionsMutex, [this, &infoHash, &task]() {
                task->status = "Failed - Retry Scheduled";
                task->lastRetryTime = std::chrono::steady_clock::now();
                m_failedAcquisitions[infoHash] = task;
            }, "MetadataAcquisitionManager::m_failedAcquisitionsMutex");
        } catch (const utility::thread::LockTimeoutException& e) {
            unified_event::logError("BitTorrent.MetadataAcquisitionManager", e.what());
        }
    } else {
        unified_event::logWarning("BitTorrent.MetadataAcquisitionManager", "Maximum retry count reached for info hash: " +
                                types::infoHashToString(infoHash));
    }
}

} // namespace dht_hunter::bittorrent::metadata
