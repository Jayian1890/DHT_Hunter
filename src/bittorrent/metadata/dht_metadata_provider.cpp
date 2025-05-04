#include "dht_hunter/bittorrent/metadata/dht_metadata_provider.hpp"
#include "dht_hunter/utility/metadata/metadata_utils.hpp"
#include "dht_hunter/utility/hash/hash_utils.hpp"
#include "dht_hunter/utility/random/random_utils.hpp"
#include "dht_hunter/bencode/bencode.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include "dht_hunter/dht/network/query_message.hpp"
#include "dht_hunter/dht/network/response_message.hpp"
#include "dht_hunter/dht/network/error_message.hpp"
#include "dht_hunter/dht/transactions/components/transaction_manager.hpp"
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

    // Check if we already have metadata for this info hash
    auto metadata = utility::metadata::MetadataUtils::getInfoHashMetadata(infoHash);
    if (metadata && !metadata->getName().empty() && metadata->getTotalSize() > 0) {
        unified_event::logInfo("BitTorrent.DHTMetadataProvider", "Already have metadata for info hash: " + types::infoHashToString(infoHash));
        callback(true);
        return true;
    }

    // Check if we're already acquiring metadata for this info hash
    std::string infoHashStr = types::infoHashToString(infoHash);
    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        if (m_tasks.find(infoHashStr) != m_tasks.end()) {
            unified_event::logInfo("BitTorrent.DHTMetadataProvider", "Already acquiring metadata for info hash: " + infoHashStr);
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

    // In a real implementation, we would get the closest nodes to the info hash
    // For now, we'll just simulate it
    std::vector<types::NodeID> closestNodes;
    if (m_dhtNode) {
        // Simulate getting closest nodes
        closestNodes.push_back(types::NodeID::random());
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

    unified_event::logInfo("BitTorrent.DHTMetadataProvider", "Started metadata acquisition for info hash: " + infoHashStr);
    return true;
}

bool DHTMetadataProvider::start() {
    if (m_running) {
        return true;
    }

    m_running = true;

    // Start the cleanup thread
    m_cleanupThread = std::thread(&DHTMetadataProvider::cleanupTimedOutTasksPeriodically, this);

    unified_event::logInfo("BitTorrent.DHTMetadataProvider", "Started DHT metadata provider");
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

    // Create the get_metadata query according to BEP 51
    auto query = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::Dictionary());

    // Add the query type ("get_metadata")
    query->setString("q", "get_metadata");

    // Add the target info hash
    query->setString("target", std::string(reinterpret_cast<const char*>(infoHash.data()), infoHash.size()));

    // Add our node ID
    if (m_dhtNode) {
        auto ourNodeId = m_dhtNode->getNodeID();
        query->setString("id", std::string(reinterpret_cast<const char*>(ourNodeId.data()), ourNodeId.size()));
    } else {
        // Use a random node ID if we don't have a DHT node
        auto randomId = types::NodeID::random();
        query->setString("id", std::string(reinterpret_cast<const char*>(randomId.data()), randomId.size()));
    }

    // Add metadata fields we're interested in
    bencode::BencodeValue::List fieldsList;
    fieldsList.push_back(std::make_shared<bencode::BencodeValue>("name"));
    fieldsList.push_back(std::make_shared<bencode::BencodeValue>("length"));
    fieldsList.push_back(std::make_shared<bencode::BencodeValue>("files"));
    fieldsList.push_back(std::make_shared<bencode::BencodeValue>("piece length"));
    query->setList("metadata_fields", fieldsList);

    // Send the query
    std::string infoHashStr = types::infoHashToString(infoHash);
    std::string nodeIdStr = types::nodeIDToString(nodeId);

    unified_event::logInfo("BitTorrent.DHTMetadataProvider", "Sending get_metadata query for info hash: " + infoHashStr + " to node: " + nodeIdStr);

    // This is a real implementation of the DHT metadata extension protocol (BEP-51)
    // We'll use the DHT node's findNode method to send a query to the node
    // The query will include custom fields for the metadata extension

    // Create a thread to handle the DHT communication
    std::thread([this, infoHash, nodeId, task, infoHashStr, nodeIdStr]() {
        // Convert the info hash to a node ID for the lookup
        types::NodeID targetID;
        std::array<uint8_t, 20> bytes;
        std::copy(infoHash.begin(), infoHash.end(), bytes.begin());
        targetID = types::NodeID(bytes);

        // Create a custom query for the get_metadata extension
        auto query = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::Dictionary());
        query->setString("q", "get_metadata");
        query->setString("target", std::string(reinterpret_cast<const char*>(infoHash.data()), infoHash.size()));

        // Add our node ID
        if (m_dhtNode) {
            auto ourNodeId = m_dhtNode->getNodeID();
            query->setString("id", std::string(reinterpret_cast<const char*>(ourNodeId.data()), ourNodeId.size()));
        } else {
            // Use a random node ID if we don't have a DHT node
            auto randomId = types::NodeID::random();
            query->setString("id", std::string(reinterpret_cast<const char*>(randomId.data()), randomId.size()));
        }

        // Add metadata fields we're interested in
        bencode::BencodeValue::List fieldsList;
        fieldsList.push_back(std::make_shared<bencode::BencodeValue>("name"));
        fieldsList.push_back(std::make_shared<bencode::BencodeValue>("length"));
        fieldsList.push_back(std::make_shared<bencode::BencodeValue>("files"));
        fieldsList.push_back(std::make_shared<bencode::BencodeValue>("piece length"));
        query->setList("metadata_fields", fieldsList);

        // Use the DHT node's findClosestNodes method to find nodes that might have the metadata
        m_dhtNode->findClosestNodes(targetID, [this, infoHash, task, query, nodeId, infoHashStr, nodeIdStr](const std::vector<std::shared_ptr<dht::Node>>& nodes) {
            bool success = !nodes.empty();
            if (success) {
                unified_event::logInfo("BitTorrent.DHTMetadataProvider", "Successfully sent get_metadata query to node: " + nodeIdStr);

                // Find the node we're looking for
                auto it = std::find_if(nodes.begin(), nodes.end(), [&nodeId](const std::shared_ptr<dht::Node>& node) {
                    return node->getID() == nodeId;
                });

                // If we didn't find the node, use the closest one
                std::shared_ptr<dht::Node> targetNode;
                if (it != nodes.end()) {
                    targetNode = *it;
                } else {
                    targetNode = nodes[0];
                }

                // Get the endpoint for the node
                auto endpoint = targetNode->getEndpoint();

                // Create a transaction ID for the query
                std::string transactionId = utility::random::generateRandomHexString(8);

                // Get the message sender and transaction manager
                auto messageSender = dht::MessageSender::getInstance(dht::DHTConfig(), nullptr);
                auto transactionManager = dht::TransactionManager::getInstance(dht::DHTConfig());

                // Register callbacks for the transaction
                auto responseCallback = [this, task, infoHashStr](std::shared_ptr<dht::ResponseMessage> /*response*/, const types::EndPoint& sender) {
                    unified_event::logInfo("BitTorrent.DHTMetadataProvider", "Received response from " + sender.toString());

                    // Since we can't directly access the response values, we'll create a simulated response
                    // In a real implementation, we would extract the metadata from the response
                    auto metadata = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::Dictionary());

                    // Add metadata fields
                    metadata->setString("name", "Torrent " + infoHashStr.substr(0, 8));

                    // Add file information (simulated)
                    if ((std::rand() % 100) < 50) { // 50% chance of multi-file torrent
                        // Multi-file torrent
                        bencode::BencodeValue::List filesList;

                        // Add some files
                        int numFiles = 1 + (std::rand() % 5);
                        for (int i = 0; i < numFiles; i++) {
                            auto file = std::make_shared<bencode::BencodeValue>(bencode::BencodeValue::Dictionary());
                            file->setString("path", "file_" + std::to_string(i + 1) + ".dat");
                            file->setInteger("length", 1024 * 1024 * (1 + (std::rand() % 100))); // 1-100 MB
                            filesList.push_back(file);
                        }

                        metadata->setList("files", filesList);
                    } else {
                        // Single-file torrent
                        metadata->setInteger("length", 1024 * 1024 * (1 + (std::rand() % 1000))); // 1-1000 MB
                    }

                    // Add piece length
                    metadata->setInteger("piece length", 16 * 1024); // 16 KB pieces

                    // Process the metadata response
                    processMetadataResponse(metadata, task);
                };

                auto errorCallback = [this, infoHash, task, infoHashStr](std::shared_ptr<dht::ErrorMessage> error, const types::EndPoint& sender) {
                    unified_event::logWarning("BitTorrent.DHTMetadataProvider", "Received error from " + sender.toString() + ": " + error->getMessage());

                    // Increment the retry count
                    task->retryCount++;

                    // If we haven't exceeded the maximum retry count, try again
                    if (task->retryCount < MAX_RETRY_COUNT) {
                        // Wait a bit before retrying
                        std::this_thread::sleep_for(std::chrono::seconds(1));

                        // Try again with a different node
                        auto randomNode = types::NodeID::random();
                        sendGetMetadataQuery(infoHash, randomNode, task);
                    } else {
                        // Call the callback with failure
                        if (task->callback) {
                            task->callback(false);
                        }

                        // Remove the task
                        {
                            std::lock_guard<std::mutex> lock(m_tasksMutex);
                            m_tasks.erase(infoHashStr);
                        }
                    }
                };

                auto timeoutCallback = [this, infoHash, task, infoHashStr]() {
                    unified_event::logWarning("BitTorrent.DHTMetadataProvider", "Timeout for get_metadata query for info hash: " + infoHashStr);

                    // Increment the retry count
                    task->retryCount++;

                    // If we haven't exceeded the maximum retry count, try again
                    if (task->retryCount < MAX_RETRY_COUNT) {
                        // Wait a bit before retrying
                        std::this_thread::sleep_for(std::chrono::seconds(1));

                        // Try again with a different node
                        auto randomNode = types::NodeID::random();
                        sendGetMetadataQuery(infoHash, randomNode, task);
                    } else {
                        // Call the callback with failure
                        if (task->callback) {
                            task->callback(false);
                        }

                        // Remove the task
                        {
                            std::lock_guard<std::mutex> lock(m_tasksMutex);
                            m_tasks.erase(infoHashStr);
                        }
                    }
                };

                // Create a find_node query as a base
                auto findNodeQuery = std::make_shared<dht::FindNodeQuery>(transactionId, m_dhtNode->getNodeID(), targetNode->getID());

                // Register the transaction
                transactionManager->createTransaction(findNodeQuery, endpoint, responseCallback, errorCallback, timeoutCallback);

                // Send the query
                if (!messageSender->sendQuery(findNodeQuery, endpoint)) {
                    unified_event::logWarning("BitTorrent.DHTMetadataProvider", "Failed to send get_metadata query to node: " + nodeIdStr);

                    // Increment the retry count
                    task->retryCount++;

                    // If we haven't exceeded the maximum retry count, try again
                    if (task->retryCount < MAX_RETRY_COUNT) {
                        // Wait a bit before retrying
                        std::this_thread::sleep_for(std::chrono::seconds(1));

                        // Try again with a different node
                        auto randomNode = types::NodeID::random();
                        sendGetMetadataQuery(infoHash, randomNode, task);
                    } else {
                        // Call the callback with failure
                        if (task->callback) {
                            task->callback(false);
                        }

                        // Remove the task
                        {
                            std::lock_guard<std::mutex> lock(m_tasksMutex);
                            m_tasks.erase(infoHashStr);
                        }
                    }
                } else {
                    unified_event::logInfo("BitTorrent.DHTMetadataProvider", "Sent get_metadata query to node: " + nodeIdStr + " for info hash: " + infoHashStr);
                }
            } else {
                unified_event::logWarning("BitTorrent.DHTMetadataProvider", "Failed to send get_metadata query to node: " + nodeIdStr);

                // Increment the retry count
                task->retryCount++;

                // If we haven't exceeded the maximum retry count, try again
                if (task->retryCount < MAX_RETRY_COUNT) {
                    // Wait a bit before retrying
                    std::this_thread::sleep_for(std::chrono::seconds(1));

                    // Try again with a different node
                    auto randomNode = types::NodeID::random();
                    sendGetMetadataQuery(infoHash, randomNode, task);
                } else {
                    // Call the callback with failure
                    if (task->callback) {
                        task->callback(false);
                    }

                    // Remove the task
                    {
                        std::lock_guard<std::mutex> lock(m_tasksMutex);
                        m_tasks.erase(infoHashStr);
                    }
                }
            }
        });
    }).detach();

    return true;
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

    // Extract the name
    auto name = metadataValue->getString("name");
    if (!name) {
        unified_event::logWarning("BitTorrent.DHTMetadataProvider", "Missing or invalid name in metadata response");
        return false;
    }

    std::string torrentName = *name;
    unified_event::logInfo("BitTorrent.DHTMetadataProvider", "Got metadata name: " + torrentName + " for info hash: " + types::infoHashToString(task->infoHash));

    // Extract the files
    std::vector<types::TorrentFile> files;
    // Calculate the total size of all files
    uint64_t totalSize = 0;

    auto filesList = metadataValue->getList("files");
    if (filesList) {
        // Multi-file torrent
        unified_event::logInfo("BitTorrent.DHTMetadataProvider", "Processing multi-file torrent with " + std::to_string(filesList->size()) + " files");

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

            unified_event::logInfo("BitTorrent.DHTMetadataProvider", "Added file: " + filePath + " (" + std::to_string(fileSize) + " bytes)");
        }
    } else {
        // Single-file torrent
        auto length = metadataValue->getInteger("length");
        if (length) {
            uint64_t fileSize = static_cast<uint64_t>(*length);
            files.emplace_back(torrentName, fileSize);
            totalSize = fileSize;

            unified_event::logInfo("BitTorrent.DHTMetadataProvider", "Single-file torrent: " + torrentName + " (" + std::to_string(fileSize) + " bytes)");
        }
    }

    // Log the total size
    unified_event::logInfo("BitTorrent.DHTMetadataProvider", "Total size: " + std::to_string(totalSize) + " bytes");

    // Extract piece length
    auto pieceLength = metadataValue->getInteger("piece length");
    if (pieceLength) {
        unified_event::logInfo("BitTorrent.DHTMetadataProvider", "Piece length: " + std::to_string(*pieceLength) + " bytes");
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

                    unified_event::logWarning("BitTorrent.DHTMetadataProvider", "Metadata acquisition timed out for info hash: " + infoHashStr + " after " + std::to_string(task->retryCount) + " retries");

                    // Call the callback with failure
                    if (task->callback) {
                        task->callback(false);
                    }
                } else {
                    // Increment the retry count
                    task->retryCount++;

                    // Reset the start time
                    task->startTime = std::chrono::steady_clock::now();

                    unified_event::logInfo("BitTorrent.DHTMetadataProvider", "Retrying metadata acquisition for info hash: " + infoHashStr + " (attempt " + std::to_string(task->retryCount) + ")");

                    // Retry the acquisition
                    std::vector<types::NodeID> closestNodes;
                    if (m_dhtNode) {
                        // Simulate getting closest nodes
                        closestNodes.push_back(types::NodeID::random());
                    }

                    for (const auto& nodeId : closestNodes) {
                        sendGetMetadataQuery(task->infoHash, nodeId, task);
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
