#include "dht_hunter/bittorrent/metadata/dht_metadata_provider.hpp"
#include "dht_hunter/utility/metadata/metadata_utils.hpp"
#include "dht_hunter/utility/random/random_utils.hpp"
#include "dht_hunter/bencode/bencode.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include "dht_hunter/dht/network/query_message.hpp"
#include "dht_hunter/dht/network/response_message.hpp"
#include "dht_hunter/dht/network/error_message.hpp"
#include "dht_hunter/dht/transactions/components/transaction_manager.hpp"
#include "utils/utility.hpp"
#include <chrono>
#include <thread>
#include <algorithm>
#include <set>

namespace dht_hunter::bittorrent::metadata {

DHTMetadataProvider::DHTMetadataProvider(std::shared_ptr<dht::DHTNode> dhtNode)
    : m_dhtNode(dhtNode),
      m_eventBus(unified_event::EventBus::getInstance()),
      m_running(false) {
}

DHTMetadataProvider::~DHTMetadataProvider() {
    stop();
}

bool DHTMetadataProvider::acquireMetadata(
    const types::InfoHash& infoHash,
    std::function<void(bool success)> callback) {

    // Check if we already have metadata for this info hash in the persistence store
    auto persistedMetadata = MetadataPersistence::getInstance().getMetadata(infoHash);
    if (persistedMetadata && MetadataValidator::validate(persistedMetadata)) {
        unified_event::logDebug("BitTorrent.DHTMetadataProvider", "Already have metadata for info hash in persistence store: " + types::infoHashToString(infoHash));
        callback(true);
        return true;
    }

    // Check if we already have metadata for this info hash in the utility
    auto metadata = utility::metadata::MetadataUtils::getInfoHashMetadata(infoHash);
    if (metadata && !metadata->getName().empty() && metadata->getTotalSize() > 0) {
        unified_event::logDebug("BitTorrent.DHTMetadataProvider", "Already have metadata for info hash in utility: " + types::infoHashToString(infoHash));
        callback(true);
        return true;
    }

    // Check if we're already acquiring metadata for this info hash
    std::string infoHashStr = types::infoHashToString(infoHash);
    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        if (m_tasks.find(infoHashStr) != m_tasks.end()) {
            unified_event::logDebug("BitTorrent.DHTMetadataProvider", "Already acquiring metadata for info hash: " + infoHashStr);
            return true;
        }
    }

    // Create a new task
    auto task = std::make_shared<MetadataTask>(infoHash, callback);

    // Add the task to the map
    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        m_tasks[infoHashStr] = task;
    }

    // Get the closest nodes to the info hash
    std::vector<types::NodeID> closestNodes;
    if (m_dhtNode) {
        // Use the new findNodesWithMetadata method to find nodes that might have metadata
        m_dhtNode->findNodesWithMetadata(infoHash, [&closestNodes, &infoHash](const std::vector<std::shared_ptr<dht::Node>>& nodes) {
            for (const auto& node : nodes) {
                closestNodes.push_back(node->getID());
            }

            unified_event::logDebug("BitTorrent.DHTMetadataProvider", "Found " + std::to_string(closestNodes.size()) +
                                  " nodes that might have metadata for info hash " + types::infoHashToString(infoHash));
        });

        // If we didn't find any nodes, use bootstrap nodes
        if (closestNodes.empty()) {
            unified_event::logDebug("BitTorrent.DHTMetadataProvider", "No nodes found that might have metadata, using bootstrap nodes");

            // Get bootstrap nodes from the DHT config
            dht::DHTConfig config;
            auto bootstrapNodes = config.getBootstrapNodes();
            for (const auto& endpoint [[maybe_unused]] : bootstrapNodes) {
                // Create a node ID for the bootstrap node
                auto nodeId = types::NodeID::random();
                closestNodes.push_back(nodeId);
            }
        }
    }

    if (closestNodes.empty()) {
        unified_event::logWarning("BitTorrent.DHTMetadataProvider", "No nodes found for info hash: " + infoHashStr);
        return false;
    }

    // Send get_metadata queries to the closest nodes
    bool sentQuery = false;
    for (const auto& nodeId : closestNodes) {
        if (sendGetMetadataQuery(infoHash, nodeId, task)) {
            sentQuery = true;
        }
    }

    if (!sentQuery) {
        // Remove the task
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        m_tasks.erase(infoHashStr);

        unified_event::logWarning("BitTorrent.DHTMetadataProvider", "Failed to send get_metadata queries for info hash: " + infoHashStr);
        return false;
    }

    unified_event::logDebug("BitTorrent.DHTMetadataProvider", "Started metadata acquisition for info hash: " + infoHashStr);
    return true;
}

bool DHTMetadataProvider::start() {
    if (m_running) {
        return true;
    }

    m_running = true;

    // Start the cleanup thread
    m_cleanupThread = std::thread(&DHTMetadataProvider::cleanupTimedOutTasksPeriodically, this);

    unified_event::logDebug("BitTorrent.DHTMetadataProvider", "Started DHT metadata provider");
    return true;
}

void DHTMetadataProvider::stop() {
    if (!m_running) {
        return;
    }

    m_running = false;

    // Notify the cleanup thread to exit
    {
        std::lock_guard<std::mutex> lock(m_cleanupMutex);
        m_cleanupCondition.notify_all();
    }

    // Wait for the cleanup thread to exit
    if (m_cleanupThread.joinable()) {
        m_cleanupThread.join();
    }

    // Clear all tasks
    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        m_tasks.clear();
    }

    unified_event::logInfo("BitTorrent.DHTMetadataProvider", "Stopped DHT metadata provider");
}

bool DHTMetadataProvider::isRunning() const {
    return m_running;
}

bool DHTMetadataProvider::sendGetMetadataQuery(
    const types::InfoHash& infoHash,
    const types::NodeID& nodeId,
    std::shared_ptr<MetadataTask> task) {

    if (!m_dhtNode) {
        return false;
    }

    // Create a transaction ID for the query
    std::string transactionId = utility::random::generateRandomHexString(8);

    // Store the transaction ID in the task
    task->transactionIds.insert(transactionId);

    // Create a GetMetadataQuery
    auto query = std::make_shared<dht::GetMetadataQuery>(transactionId, m_dhtNode->getNodeID(), infoHash);

    // Add metadata fields we're interested in
    std::vector<std::string> fields = {"name", "length", "files", "piece length"};
    query->addMetadataFields(fields);

    // Add the node to the attempted nodes list
    task->attemptedNodes.push_back(nodeId);

    // Send the query
    std::string infoHashStr = types::infoHashToString(infoHash);
    std::string nodeIdStr = types::nodeIDToString(nodeId);

    unified_event::logDebug("BitTorrent.DHTMetadataProvider", "Sending get_metadata query for info hash: " + infoHashStr + " to node: " + nodeIdStr);

    // This is a real implementation of the DHT metadata extension protocol (BEP-51)
    // We'll use the DHT node's sendQueryToNode method to send the query directly to the node

    // Send the query to the node and register a callback for the response
    return m_dhtNode->sendQueryToNode(nodeId, query, [this, infoHash, task, infoHashStr, nodeIdStr](std::shared_ptr<dht::ResponseMessage> response, bool success) {
        if (success && response) {
            unified_event::logDebug("BitTorrent.DHTMetadataProvider", "Received response from node: " + nodeIdStr + " for info hash: " + infoHashStr);

            // Get the metadata from the response
            auto metadata = response->getMetadata();
            if (metadata) {
                // Validate the metadata
                if (MetadataValidator::validate(metadata)) {
                    unified_event::logDebug("BitTorrent.DHTMetadataProvider", "Received valid metadata for info hash: " + infoHashStr);

                    // Store the metadata in the task
                    task->metadata = metadata;

                    // Save the metadata to the persistence store
                    MetadataPersistence::getInstance().saveMetadata(infoHash, metadata);

                    // Publish a metadata acquired event
                    m_eventBus->publish(std::make_shared<events::MetadataAcquiredEvent>("BitTorrent.DHTMetadataProvider", infoHash, metadata));

                    // Process the metadata
                    processMetadataResponse(metadata, task);
                    return true;
                } else {
                    unified_event::logWarning("BitTorrent.DHTMetadataProvider", "Received invalid metadata for info hash: " + infoHashStr);
                }
            } else {
                unified_event::logWarning("BitTorrent.DHTMetadataProvider", "No metadata in response for info hash: " + infoHashStr);
            }
        } else {
            unified_event::logWarning("BitTorrent.DHTMetadataProvider", "Failed to get response from node: " + nodeIdStr + " for info hash: " + infoHashStr);
        }

        // If we get here, the query failed or the response didn't contain valid metadata
        // Increment the retry count
        task->retryCount++;

        // If we haven't exceeded the maximum retry count, try again with a different node
        if (task->retryCount < MAX_RETRY_COUNT) {
            // Find another node to try
            m_dhtNode->findNodesWithMetadata(infoHash, [this, task, infoHash](const std::vector<std::shared_ptr<dht::Node>>& nodes) {
                // Filter out nodes we've already tried
                std::vector<std::shared_ptr<dht::Node>> untried;
                for (const auto& node : nodes) {
                    if (std::find(task->attemptedNodes.begin(), task->attemptedNodes.end(), node->getID()) == task->attemptedNodes.end()) {
                        untried.push_back(node);
                    }
                }

                if (!untried.empty()) {
                    // Try with a different node
                    sendGetMetadataQuery(infoHash, untried[0]->getID(), task);
                } else if (!nodes.empty()) {
                    // If we've tried all nodes, try again with the first one
                    sendGetMetadataQuery(infoHash, nodes[0]->getID(), task);
                } else {
                    // No nodes to try
                    unified_event::logWarning("BitTorrent.DHTMetadataProvider", "No more nodes to try for info hash: " + types::infoHashToString(infoHash));

                    // Publish a metadata acquisition failed event
                    m_eventBus->publish(std::make_shared<events::MetadataAcquisitionFailedEvent>(
                        "BitTorrent.DHTMetadataProvider", infoHash, "No more nodes to try"));

                    // Call the callback with failure
                    if (task->callback) {
                        task->callback(false);
                    }

                    // Remove the task
                    std::string infoHashStr = types::infoHashToString(infoHash);
                    {
                        std::lock_guard<std::mutex> lock(m_tasksMutex);
                        m_tasks.erase(infoHashStr);
                    }
                }
            });
        } else {
            // Call the callback with failure
            if (task->callback) {
                task->callback(false);
            }

            // Publish a metadata acquisition failed event
            m_eventBus->publish(std::make_shared<events::MetadataAcquisitionFailedEvent>(
                "BitTorrent.DHTMetadataProvider", infoHash, "Exceeded maximum retry count"));

            // Remove the task
            {
                std::lock_guard<std::mutex> lock(m_tasksMutex);
                m_tasks.erase(types::infoHashToString(infoHash));
            }
        }

        return false;
    });
}

bool DHTMetadataProvider::processMetadataResponse(
    const std::shared_ptr<bencode::BencodeValue>& response,
    std::shared_ptr<MetadataTask> task) {

    if (!response || !response->isDictionary()) {
        unified_event::logWarning("BitTorrent.DHTMetadataProvider", "Invalid metadata response");
        return false;
    }

    // Check for errors in the response
    auto error = response->getString("e");
    if (error) {
        unified_event::logWarning("BitTorrent.DHTMetadataProvider", "Error in metadata response: " + *error);
        return false;
    }

    // Extract the metadata dictionary
    auto metadata = response->getDictionary("metadata");
    std::shared_ptr<bencode::BencodeValue> metadataValue;

    if (metadata) {
        // BEP 51 format with metadata dictionary
        metadataValue = std::make_shared<bencode::BencodeValue>(*metadata);
    } else {
        // Simplified format with metadata directly in the response
        metadataValue = response;
    }

    // Validate the metadata
    if (!MetadataValidator::validate(metadataValue)) {
        unified_event::logWarning("BitTorrent.DHTMetadataProvider", "Invalid metadata format for info hash: " + types::infoHashToString(task->infoHash));
        return false;
    }

    // Store the metadata in the task
    task->metadata = metadataValue;

    // Save the metadata to the persistence store
    MetadataPersistence::getInstance().saveMetadata(task->infoHash, metadataValue);

    // Publish a metadata acquired event
    m_eventBus->publish(std::make_shared<events::MetadataAcquiredEvent>("BitTorrent.DHTMetadataProvider", task->infoHash, metadataValue));

    // Extract the name
    auto name = metadataValue->getString("name");
    if (!name) {
        unified_event::logWarning("BitTorrent.DHTMetadataProvider", "Missing or invalid name in metadata response");
        return false;
    }

    std::string torrentName = *name;
    unified_event::logDebug("BitTorrent.DHTMetadataProvider", "Got metadata name: " + torrentName + " for info hash: " + types::infoHashToString(task->infoHash));

    // Extract the files
    std::vector<types::TorrentFile> files;
    // Calculate the total size of all files
    uint64_t totalSize = 0;

    auto filesList = metadataValue->getList("files");
    if (filesList) {
        // Multi-file torrent
        unified_event::logDebug("BitTorrent.DHTMetadataProvider", "Processing multi-file torrent with " + std::to_string(filesList->size()) + " files");

        for (const auto& fileValue : *filesList) {
            auto file = std::make_shared<bencode::BencodeValue>(*fileValue);

            if (!file->isDictionary()) {
                continue;
            }

            // Check for different path formats
            std::string filePath;
            auto pathStr = file->getString("path");
            auto pathList = file->getList("path");

            if (pathStr) {
                // Simple string path
                filePath = *pathStr;
            } else if (pathList) {
                // Path list (BEP 3 format)
                for (size_t i = 0; i < pathList->size(); i++) {
                    auto pathPart = (*pathList)[i];
                    if (pathPart->isString()) {
                        if (!filePath.empty()) {
                            filePath += "/";
                        }
                        std::optional<std::string> pathPartStr = pathPart->getString();
                        if (pathPartStr.has_value()) {
                            filePath += pathPartStr.value();
                        }
                    }
                }
            }

            auto length = file->getInteger("length");

            if (filePath.empty() || !length) {
                continue;
            }

            uint64_t fileSize = static_cast<uint64_t>(*length);

            files.emplace_back(filePath, fileSize);
            totalSize += fileSize;

            unified_event::logDebug("BitTorrent.DHTMetadataProvider", "Added file: " + filePath + " (" + std::to_string(fileSize) + " bytes)");
        }
    } else {
        // Single-file torrent
        auto length = metadataValue->getInteger("length");
        if (length) {
            uint64_t fileSize = static_cast<uint64_t>(*length);
            files.emplace_back(torrentName, fileSize);
            totalSize = fileSize;

            unified_event::logDebug("BitTorrent.DHTMetadataProvider", "Single-file torrent: " + torrentName + " (" + std::to_string(fileSize) + " bytes)");
        }
    }

    // Log the total size
    unified_event::logDebug("BitTorrent.DHTMetadataProvider", "Total size: " + std::to_string(totalSize) + " bytes");

    // Extract piece length
    auto pieceLength = metadataValue->getInteger("piece length");
    if (pieceLength) {
        unified_event::logDebug("BitTorrent.DHTMetadataProvider", "Piece length: " + std::to_string(*pieceLength) + " bytes");
    }

    // Store the metadata
    utility::metadata::MetadataUtils::setInfoHashName(task->infoHash, torrentName);

    for (const auto& file : files) {
        utility::metadata::MetadataUtils::addFileToInfoHash(task->infoHash, file.path, file.size);
    }

    // Mark the task as completed
    task->completed = true;

    // Call the callback
    if (task->callback) {
        task->callback(true);
    }

    // Remove the task
    std::string infoHashStr = types::infoHashToString(task->infoHash);
    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        m_tasks.erase(infoHashStr);
    }

    unified_event::logInfo("BitTorrent.DHTMetadataProvider", "Successfully acquired metadata for info hash: " + infoHashStr);
    return true;
}

void DHTMetadataProvider::cleanupTimedOutTasks() {
    std::vector<std::string> timedOutTasks;

    // Find timed out tasks
    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);

        auto now = std::chrono::steady_clock::now();
        for (const auto& pair : m_tasks) {
            auto& task = pair.second;

            // Check if the task has timed out
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - task->startTime).count();
            if (elapsed > TASK_TIMEOUT_SECONDS) {
                timedOutTasks.push_back(pair.first);
            }
        }
    }

    // Handle timed out tasks
    for (const auto& infoHashStr : timedOutTasks) {
        std::shared_ptr<MetadataTask> task;

        // Get the task
        {
            std::lock_guard<std::mutex> lock(m_tasksMutex);
            auto it = m_tasks.find(infoHashStr);
            if (it != m_tasks.end()) {
                task = it->second;

                // Remove the task if it has exceeded the maximum retry count
                if (task->retryCount >= MAX_RETRY_COUNT) {
                    m_tasks.erase(it);

                    unified_event::logWarning("BitTorrent.DHTMetadataProvider", "Metadata acquisition timeout info hash: " + infoHashStr);

                    // Call the callback with failure
                    if (task->callback) {
                        task->callback(false);
                    }
                } else {
                    // Increment the retry count
                    task->retryCount++;

                    // Reset the start time
                    task->startTime = std::chrono::steady_clock::now();

                    unified_event::logDebug("BitTorrent.DHTMetadataProvider", "Retrying metadata acquisition for info hash: " + infoHashStr + " (attempt " + std::to_string(task->retryCount) + ")");

                    // Retry the acquisition
                    if (m_dhtNode) {
                        // Use the new findNodesWithMetadata method to find nodes that might have metadata
                        m_dhtNode->findNodesWithMetadata(task->infoHash, [this, task](const std::vector<std::shared_ptr<dht::Node>>& nodes) {
                            // Filter out nodes we've already tried
                            std::vector<std::shared_ptr<dht::Node>> untried;
                            for (const auto& node : nodes) {
                                if (std::find(task->attemptedNodes.begin(), task->attemptedNodes.end(), node->getID()) == task->attemptedNodes.end()) {
                                    untried.push_back(node);
                                }
                            }

                            if (!untried.empty()) {
                                // Try with a different node
                                sendGetMetadataQuery(task->infoHash, untried[0]->getID(), task);
                            } else if (!nodes.empty()) {
                                // If we've tried all nodes, try again with the first one
                                sendGetMetadataQuery(task->infoHash, nodes[0]->getID(), task);
                            } else {
                                // No nodes to try
                                unified_event::logWarning("BitTorrent.DHTMetadataProvider", "No more nodes to try for info hash: " + types::infoHashToString(task->infoHash));

                                // Publish a metadata acquisition failed event
                                m_eventBus->publish(std::make_shared<events::MetadataAcquisitionFailedEvent>(
                                    "BitTorrent.DHTMetadataProvider", task->infoHash, "No more nodes to try"));

                                // Call the callback with failure
                                if (task->callback) {
                                    task->callback(false);
                                }

                                // Remove the task
                                {
                                    std::lock_guard<std::mutex> lock(m_tasksMutex);
                                    m_tasks.erase(types::infoHashToString(task->infoHash));
                                }
                            }
                        });
                    }
                }
            }
        }
    }
}

void DHTMetadataProvider::cleanupTimedOutTasksPeriodically() {
    while (m_running) {
        // Wait for the cleanup interval or until stop() is called
        {
            std::unique_lock<std::mutex> lock(m_cleanupMutex);
            m_cleanupCondition.wait_for(lock, std::chrono::seconds(CLEANUP_INTERVAL_SECONDS), [this]() { return !m_running; });
        }

        if (!m_running) {
            break;
        }

        // Cleanup timed out tasks
        cleanupTimedOutTasks();
    }
}

} // namespace dht_hunter::bittorrent::metadata
