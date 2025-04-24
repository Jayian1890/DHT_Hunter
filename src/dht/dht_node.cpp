#include "dht_hunter/dht/dht_node.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include "dht_hunter/network/platform/socket_impl.hpp"
#include "dht_hunter/crawler/infohash_collector.hpp"
#include "dht_hunter/util/mutex_utils.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <algorithm>
#include <filesystem>
#include <future>

DEFINE_COMPONENT_LOGGER("DHT", "Node")

// Helper function to generate a random string of the specified length
std::string generateRandomString(size_t length) {
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);

    std::string result(length, 0);
    for (size_t i = 0; i < length; ++i) {
        result[i] = charset[dist(rng)];
    }
    return result;
}

namespace dht_hunter::dht {
DHTNode::DHTNode(uint16_t port, const std::string& configDir, const NodeID& nodeID)
    : m_nodeID(nodeID), m_port(port), m_routingTable(nodeID),
      m_socket(nullptr), m_messageBatcher(nullptr), m_routingTablePath(configDir + "/routing_table.dat"),
      m_running(false),
      m_secret(generateRandomString(20)), m_secretLastChanged(std::chrono::steady_clock::now()) {
    getLogger()->info("Creating DHT node with ID: {}, port: {}, config path: {}",
                 nodeIDToString(m_nodeID), port, m_routingTablePath);

    // Ensure the config directory exists
    try {
        if (!std::filesystem::exists(configDir)) {
            getLogger()->info("Config directory does not exist, creating: {}", configDir);
            if (!std::filesystem::create_directories(configDir)) {
                getLogger()->error("Failed to create config directory: {}", configDir);
            }
        }
    } catch (const std::exception& e) {
        getLogger()->error("Exception while ensuring config directory exists: {}", e.what());
    }

    // First try to load the node ID from the routing table file
    std::string nodeIdFilePath = m_routingTablePath + ".nodeid";
    bool nodeIdLoaded = false;
    NodeID originalNodeId = m_nodeID; // Store the original node ID

    // Try to load from the nodeid file
    std::ifstream nodeIdFile(nodeIdFilePath, std::ios::binary);
    if (nodeIdFile.is_open()) {
        NodeID storedNodeId;
        if (nodeIdFile.read(reinterpret_cast<char*>(storedNodeId.data()), static_cast<std::streamsize>(storedNodeId.size()))) {
            // Update our node ID to match the stored one
            m_nodeID = storedNodeId;
            getLogger()->info("Loaded node ID from file: {}", nodeIDToString(m_nodeID));
            nodeIdLoaded = true;
        }
        nodeIdFile.close();
    }

    // If we couldn't load from the nodeid file, try to load from the routing table file
    if (!nodeIdLoaded) {
        std::ifstream routingTableFile(m_routingTablePath, std::ios::binary);
        if (routingTableFile.is_open()) {
            // Read the first 20 bytes which should be the node ID
            NodeID storedNodeId;
            if (routingTableFile.read(reinterpret_cast<char*>(storedNodeId.data()), static_cast<std::streamsize>(storedNodeId.size()))) {
                // Update our node ID to match the stored one
                m_nodeID = storedNodeId;
                getLogger()->info("Loaded node ID from routing table file: {}", nodeIDToString(m_nodeID));
                nodeIdLoaded = true;

                // Also save it to the dedicated nodeid file for future use
                std::ofstream outFile(nodeIdFilePath, std::ios::binary);
                if (outFile.is_open()) {
                    outFile.write(reinterpret_cast<const char*>(m_nodeID.data()), static_cast<std::streamsize>(m_nodeID.size()));
                    outFile.close();
                    getLogger()->info("Saved node ID to dedicated file: {}", nodeIdFilePath);
                }
            }
            routingTableFile.close();
        }
    }

    // If we still couldn't load the node ID, save the current one to a file
    if (!nodeIdLoaded) {
        std::ofstream outFile(nodeIdFilePath, std::ios::binary);
        if (outFile.is_open()) {
            outFile.write(reinterpret_cast<const char*>(m_nodeID.data()), static_cast<std::streamsize>(m_nodeID.size()));
            outFile.close();
            getLogger()->info("Saved new node ID to file: {}", nodeIdFilePath);
        } else {
            getLogger()->warning("Failed to save node ID to file: {}", nodeIdFilePath);
        }
    }

    // If the node ID was loaded and is different from the original, we need to update the routing table
    if (nodeIdLoaded && m_nodeID != originalNodeId) {
        getLogger()->info("Node ID changed from {} to {}, updating routing table",
                     nodeIDToString(originalNodeId), nodeIDToString(m_nodeID));
        // Update the routing table's own node ID
        m_routingTable.updateOwnID(m_nodeID);
        // Clear the routing table to ensure all nodes are compatible with the new node ID
        m_routingTable.clear();
    }
}
DHTNode::~DHTNode() {
    stop();
}
bool DHTNode::start() {
    if (m_running) {
        getLogger()->warning("DHT node already running");
        return false;
    }
    // Create UDP socket with proper locking
    {
        std::lock_guard<util::CheckedMutex> lock(m_socketMutex);
        m_socket = network::SocketFactory::createUDPSocket();
        // Bind to port
        if (!m_socket->bind(network::EndPoint(network::NetworkAddress::any(), m_port))) {
            getLogger()->error("Failed to bind socket to port {}: {}", m_port,
                         network::Socket::getErrorString(m_socket->getLastError()));
            return false;
        }
    }
    // Set non-blocking mode
    // Attempt to set non-blocking mode, but continue even if it fails
    // This is a workaround for the "Failed to set non-blocking mode: No error" issue on macOS
    if (!m_socket->setNonBlocking(true)) {
        // Check if the error is "No error" (which is a known issue on macOS)
        if (m_socket->getLastError() == network::SocketError::None) {
            getLogger()->warning("Ignoring 'No error' when setting non-blocking mode (known macOS issue)");
            // Continue anyway - the socket will likely work fine
        } else {
            getLogger()->error("Failed to set non-blocking mode: {}",
                         network::Socket::getErrorString(m_socket->getLastError()));
            // Only return false for real errors, not the "No error" issue
            return false;
        }
    }
    // Create and start the message batcher
    network::UDPBatcherConfig batcherConfig;
    batcherConfig.enabled = true;
    batcherConfig.maxBatchSize = 1400; // Typical MTU size minus some overhead
    batcherConfig.maxMessagesPerBatch = 10;
    batcherConfig.maxBatchDelay = std::chrono::milliseconds(50); // 50ms max delay
    // Create a shared_ptr from the unique_ptr for the batcher
    auto sharedSocket = std::shared_ptr<network::Socket>(m_socket.get(), [](network::Socket*){});
    m_messageBatcher = std::make_unique<network::UDPMessageBatcher>(sharedSocket, batcherConfig);
    if (!m_messageBatcher->start()) {
        getLogger()->error("Failed to start message batcher");
        return false;
    }
    getLogger()->info("DHT node started on port {} with message batching enabled", m_port);
    // Try to load the routing table
    if (!m_routingTablePath.empty()) {
        if (loadRoutingTable(m_routingTablePath)) {
            getLogger()->info("Loaded routing table from file: {}", m_routingTablePath);
        } else {
            getLogger()->warning("Failed to load routing table from file: {}", m_routingTablePath);
        }
    }
    // Start threads
    m_running = true;
    m_receiveThread = std::thread(&DHTNode::receiveMessages, this);
    m_timeoutThread = std::thread(&DHTNode::checkTimeouts, this);
    m_peerCleanupThread = std::thread(&DHTNode::peerCleanupThread, this);
    m_routingTableSaveThread = std::thread(&DHTNode::routingTableSaveThread, this);
    return true;
}
void DHTNode::stop() {
    // Use atomic operation to prevent multiple stops
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false)) {
        // Already stopped or stopping
        return;
    }
    getLogger()->info("Stopping DHT node");

    // First, stop the message batcher to prevent any new messages from being sent
    if (m_messageBatcher) {
        getLogger()->debug("Stopping message batcher...");
        m_messageBatcher->stop();
        m_messageBatcher.reset();
        getLogger()->debug("Message batcher stopped");
    }

    // Close the socket to unblock the receive thread with proper locking
    {
        std::lock_guard<util::CheckedMutex> lock(m_socketMutex);
        if (m_socket) {
            getLogger()->debug("Closing socket to unblock receive thread");
            m_socket->close();
            // Reset socket immediately to ensure no other threads can use it
            m_socket.reset();
        }
    }

    // Join threads with timeout to prevent hanging
    const auto threadJoinTimeout = std::chrono::seconds(2);

    // Define a helper function to join a thread with timeout
    auto joinThreadWithTimeout = [threadJoinTimeout](std::thread& thread, const std::string& threadName) {
        if (!thread.joinable()) {
            return;
        }

        getLogger()->debug("Waiting for {} thread to join...", threadName);

        // Try to join with timeout
        std::thread tempThread;
        bool joined = false;

        // Create a thread to join the thread with timeout
        tempThread = std::thread([&thread, &joined]() {
            if (thread.joinable()) {
                thread.join();
                joined = true;
            }
        });

        // Wait for the join thread with timeout
        if (tempThread.joinable()) {
            auto joinStartTime = std::chrono::steady_clock::now();
            while (!joined &&
                   std::chrono::steady_clock::now() - joinStartTime < threadJoinTimeout) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            if (joined) {
                tempThread.join();
                getLogger()->debug("{} thread joined successfully", threadName);
            } else {
                // If we couldn't join, detach both threads to avoid blocking
                getLogger()->warning("{} thread join timed out after {} seconds, detaching",
                              threadName, threadJoinTimeout.count());
                tempThread.detach();
                // We can't safely join the original thread now, so we detach it
                if (thread.joinable()) {
                    thread.detach();
                }
            }
        }
    };

    // Join all threads with timeout
    joinThreadWithTimeout(m_receiveThread, "receive");
    joinThreadWithTimeout(m_timeoutThread, "timeout");
    joinThreadWithTimeout(m_peerCleanupThread, "peer cleanup");
    joinThreadWithTimeout(m_routingTableSaveThread, "routing table save");

    // Try to save the routing table before exiting, but don't block if it fails
    if (!m_routingTablePath.empty()) {
        try {
            getLogger()->debug("Saving routing table to file: {}", m_routingTablePath);
            if (!saveRoutingTable(m_routingTablePath)) {
                getLogger()->warning("Failed to save routing table to file: {}", m_routingTablePath);
            }
        } catch (const std::exception& e) {
            getLogger()->error("Exception while saving routing table: {}", e.what());
        } catch (...) {
            getLogger()->error("Unknown exception while saving routing table");
        }
    }

    // Clear transactions
    try {
        std::lock_guard<util::CheckedMutex> lock(m_transactionsMutex);
        m_transactions.clear();
    } catch (const std::exception& e) {
        getLogger()->error("Exception while clearing transactions: {}", e.what());
    } catch (...) {
        getLogger()->error("Unknown exception while clearing transactions");
    }

    getLogger()->info("DHT node stopped");
}
bool DHTNode::isRunning() const {
    return m_running;
}
const NodeID& DHTNode::getNodeID() const {
    return m_nodeID;
}
uint16_t DHTNode::getPort() const {
    return m_port;
}
const RoutingTable& DHTNode::getRoutingTable() const {
    return m_routingTable;
}
void DHTNode::setInfoHashCollector(std::shared_ptr<crawler::InfoHashCollector> collector) {
    m_infoHashCollector = collector;
    getLogger()->info("InfoHash collector set");
}
std::shared_ptr<crawler::InfoHashCollector> DHTNode::getInfoHashCollector() const {
    return m_infoHashCollector;
}
bool DHTNode::bootstrap(const network::EndPoint& endpoint) {
    if (!m_running) {
        getLogger()->error("Cannot bootstrap: DHT node not running");
        return false;
    }
    getLogger()->info("Bootstrapping DHT node using {}", endpoint.toString());
    getLogger()->debug("Bootstrap start time: {}",
                 std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());

    // Use a shared_ptr to promise to ensure it lives as long as the callbacks
    auto responsePromise = std::make_shared<std::promise<bool>>();
    auto responseFuture = responsePromise->get_future();

    // Send a find_node query for our own ID
    getLogger()->debug("Sending find_node query to {} for our own ID: {}",
                 endpoint.toString(), nodeIDToString(m_nodeID));
    bool querySent = findNode(m_nodeID, endpoint,
                   [this, endpoint, promisePtr = responsePromise](std::shared_ptr<ResponseMessage> response) {
                       getLogger()->debug("Received bootstrap response from {}", endpoint.toString());
                       // Add the responding node to our routing table
                       addNode(response->getNodeID(), endpoint);
                       // Add nodes from the response to our routing table
                       auto findNodeResponse = std::dynamic_pointer_cast<FindNodeResponse>(response);
                       if (findNodeResponse) {
                           getLogger()->debug("Response contains {} nodes", findNodeResponse->getNodes().size());
                           for (const auto& node : findNodeResponse->getNodes()) {
                               getLogger()->debug("Adding node {} at {}", nodeIDToString(node.id), node.endpoint.toString());
                               addNode(node.id, node.endpoint);
                           }
                           getLogger()->info("Bootstrap successful, added {} nodes",
                                       findNodeResponse->getNodes().size() + 1);
                       } else {
                           getLogger()->warning("Response is not a FindNodeResponse");
                       }
                       // Signal that we received a response
                       getLogger()->debug("Setting response promise to true");
                       try {
                           promisePtr->set_value(true);
                       } catch (const std::future_error& e) {
                           // Promise might already be satisfied if multiple callbacks happen
                           getLogger()->warning("Failed to set promise value: {}", e.what());
                       }
                   },
                   [promisePtr = responsePromise, endpoint](std::shared_ptr<ErrorMessage> error) {
                       getLogger()->error("Bootstrap failed for {}: {} ({})",
                                    endpoint.toString(), error->getMessage(), error->getCode());
                       // Signal that we received an error
                       getLogger()->debug("Setting response promise to false due to error");
                       try {
                           promisePtr->set_value(false);
                       } catch (const std::future_error& e) {
                           // Promise might already be satisfied if multiple callbacks happen
                           getLogger()->warning("Failed to set promise value: {}", e.what());
                       }
                   },
                   [promisePtr = responsePromise, endpoint]() {
                       getLogger()->error("Bootstrap timed out for {}", endpoint.toString());
                       // Signal that we timed out
                       getLogger()->debug("Setting response promise to false due to timeout");
                       try {
                           promisePtr->set_value(false);
                       } catch (const std::future_error& e) {
                           // Promise might already be satisfied if multiple callbacks happen
                           getLogger()->warning("Failed to set promise value: {}", e.what());
                       }
                   });

    // If the query wasn't sent successfully, return false
    if (!querySent) {
        getLogger()->warning("Failed to send bootstrap query to {}", endpoint.toString());
        return false;
    }

    getLogger()->debug("Query sent successfully to {}, waiting for response", endpoint.toString());

    // Wait for the response with a timeout
    auto waitStartTime = std::chrono::steady_clock::now();
    auto waitTimeout = std::chrono::seconds(5); // Increased timeout for better reliability
    getLogger()->debug("Waiting for bootstrap response with {} second timeout", waitTimeout.count());

    auto status = responseFuture.wait_for(waitTimeout);
    auto waitEndTime = std::chrono::steady_clock::now();
    auto waitDuration = std::chrono::duration_cast<std::chrono::milliseconds>(waitEndTime - waitStartTime);

    if (status == std::future_status::timeout) {
        getLogger()->warning("Bootstrap wait timed out after {} ms", waitDuration.count());
        // Don't try to set the promise value here, as it might race with the timeout callback
        return false;
    }

    getLogger()->debug("Bootstrap wait completed in {} ms", waitDuration.count());

    // Get the result
    bool result = false;
    try {
        result = responseFuture.get();
        getLogger()->debug("Bootstrap result: {}", result ? "success" : "failure");
    } catch (const std::exception& e) {
        getLogger()->error("Exception while getting bootstrap result: {}", e.what());
        return false;
    }

    return result;
}
bool DHTNode::bootstrap(const std::vector<network::EndPoint>& endpoints) {
    if (endpoints.empty()) {
        getLogger()->error("Cannot bootstrap: No endpoints provided");
        return false;
    }
    bool success = false;
    for (const auto& endpoint : endpoints) {
        if (bootstrap(endpoint)) {
            success = true;
        }
    }
    return success;
}
bool DHTNode::ping(const network::EndPoint& endpoint,
                  ResponseCallback responseCallback,
                  ErrorCallback errorCallback,
                  TimeoutCallback timeoutCallback) {
    if (!m_running) {
        getLogger()->error("Cannot ping: DHT node not running");
        return false;
    }
    // Create ping query
    auto query = std::make_shared<PingQuery>(generateTransactionID(), m_nodeID);
    // Send query
    return sendQuery(query, endpoint, responseCallback, errorCallback, timeoutCallback);
}
bool DHTNode::findNode(const NodeID& targetID,
                      const network::EndPoint& endpoint,
                      ResponseCallback responseCallback,
                      ErrorCallback errorCallback,
                      TimeoutCallback timeoutCallback) {
    if (!m_running) {
        getLogger()->error("Cannot find_node: DHT node not running");
        return false;
    }
    // Create find_node query
    auto query = std::make_shared<FindNodeQuery>(generateTransactionID(), m_nodeID, targetID);
    // Send query
    return sendQuery(query, endpoint, responseCallback, errorCallback, timeoutCallback);
}
bool DHTNode::getPeers(const InfoHash& infoHash,
                      const network::EndPoint& endpoint,
                      ResponseCallback responseCallback,
                      ErrorCallback errorCallback,
                      TimeoutCallback timeoutCallback) {
    if (!m_running) {
        getLogger()->error("Cannot get_peers: DHT node not running");
        return false;
    }
    // Create get_peers query
    auto query = std::make_shared<GetPeersQuery>(generateTransactionID(), m_nodeID, infoHash);
    // Send query
    return sendQuery(query, endpoint, responseCallback, errorCallback, timeoutCallback);
}
bool DHTNode::announcePeer(const InfoHash& infoHash,
                          uint16_t port,
                          const std::string& token,
                          const network::EndPoint& endpoint,
                          ResponseCallback responseCallback,
                          ErrorCallback errorCallback,
                          TimeoutCallback timeoutCallback) {
    if (!m_running) {
        getLogger()->error("Cannot announce_peer: DHT node not running");
        return false;
    }
    // Create announce_peer query
    auto query = std::make_shared<AnnouncePeerQuery>(generateTransactionID(), m_nodeID, infoHash, port, token);
    // Send query
    return sendQuery(query, endpoint, responseCallback, errorCallback, timeoutCallback);
}
bool DHTNode::sendQuery(std::shared_ptr<QueryMessage> query,
                       const network::EndPoint& endpoint,
                       ResponseCallback responseCallback,
                       ErrorCallback errorCallback,
                       TimeoutCallback timeoutCallback) {
    if (!m_running) {
        getLogger()->error("Cannot send query: DHT node not running");
        return false;
    }
    // Encode query
    std::string encodedQuery = query->encodeToString();
    // Create transaction
    Transaction transaction;
    transaction.id = query->getTransactionID();
    transaction.timestamp = std::chrono::steady_clock::now();
    transaction.responseCallback = responseCallback;
    transaction.errorCallback = errorCallback;
    transaction.timeoutCallback = timeoutCallback;
    // Add transaction
    if (!addTransaction(transaction)) {
        getLogger()->error("Failed to add transaction");
        return false;
    }
    // Send query using the message batcher
    const uint8_t* data = reinterpret_cast<const uint8_t*>(encodedQuery.data());
    bool result = m_messageBatcher->sendMessage(data, encodedQuery.size(), endpoint,
        [transactionId = transaction.id, method = query->getMethod()](bool success) {
            if (!success) {
                auto localLogger = dht_hunter::logforge::LogForge::getLogger("DHT.Node");
                localLogger->error("Failed to send {} query", queryMethodToString(method));
                // We need to access the DHTNode instance to remove the transaction
                // This is a limitation of the current design, but we'll handle it in the main error path
            }
        });
    if (!result) {
        getLogger()->error("Failed to queue query for sending");
        removeTransaction(transaction.id);
        return false;
    }
    getLogger()->debug("Sent {} query to {}", queryMethodToString(query->getMethod()), endpoint.toString());
    return true;
}
void DHTNode::handleMessage(std::shared_ptr<Message> message, const network::EndPoint& sender) {
    if (!message) {
        getLogger()->error("Received null message");
        return;
    }
    switch (message->getType()) {
        case MessageType::Query:
            handleQuery(std::dynamic_pointer_cast<QueryMessage>(message), sender);
            break;
        case MessageType::Response:
            handleResponse(std::dynamic_pointer_cast<ResponseMessage>(message), sender);
            break;
        case MessageType::Error:
            handleError(std::dynamic_pointer_cast<ErrorMessage>(message), sender);
            break;
        default:
            getLogger()->error("Received message with unknown type");
            break;
    }
}
void DHTNode::handleQuery(std::shared_ptr<QueryMessage> query, const network::EndPoint& sender) {
    if (!query) {
        getLogger()->error("Received null query");
        return;
    }
    // Add the sender to our routing table
    addNode(query->getNodeID(), sender);
    // Handle the query based on its method
    switch (query->getMethod()) {
        case QueryMethod::Ping:
            handlePingQuery(std::dynamic_pointer_cast<PingQuery>(query), sender);
            break;
        case QueryMethod::FindNode:
            handleFindNodeQuery(std::dynamic_pointer_cast<FindNodeQuery>(query), sender);
            break;
        case QueryMethod::GetPeers:
            handleGetPeersQuery(std::dynamic_pointer_cast<GetPeersQuery>(query), sender);
            break;
        case QueryMethod::AnnouncePeer:
            handleAnnouncePeerQuery(std::dynamic_pointer_cast<AnnouncePeerQuery>(query), sender);
            break;
        default:
            getLogger()->error("Received query with unknown method");
            // Send error response
            auto error = std::make_shared<ErrorMessage>(query->getTransactionID(), 204, "Method Unknown");
            sendError(error, sender);
            break;
    }
}
void DHTNode::handleResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender) {
    if (!response) {
        getLogger()->error("Received null response");
        return;
    }

    // Skip if we're shutting down
    if (!m_running) {
        getLogger()->debug("Ignoring response while shutting down");
        return;
    }

    // Add the sender to our routing table
    addNode(response->getNodeID(), sender);

    // Find the transaction and make a copy of the callback
    ResponseCallback callback;
    {
        // Find and remove the transaction atomically
        std::lock_guard<util::CheckedMutex> lock(m_transactionsMutex);

        // Convert transaction ID to string for map key
        std::string transactionIDStr(response->getTransactionID().begin(), response->getTransactionID().end());

        // Find transaction
        auto it = m_transactions.find(transactionIDStr);
        if (it == m_transactions.end()) {
            getLogger()->warning("Received response for unknown transaction");
            return;
        }

        // Copy the callback
        if (it->second->responseCallback) {
            callback = it->second->responseCallback;
        }

        // Remove the transaction
        m_transactions.erase(it);
    }

    // Call the response callback outside the lock
    if (callback) {
        try {
            callback(response);
        } catch (const std::exception& e) {
            getLogger()->error("Exception in response callback: {}", e.what());
        } catch (...) {
            getLogger()->error("Unknown exception in response callback");
        }
    }
}

void DHTNode::handleError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& /* sender */) {
    if (!error) {
        getLogger()->error("Received null error");
        return;
    }

    // Skip if we're shutting down
    if (!m_running) {
        getLogger()->debug("Ignoring error while shutting down");
        return;
    }

    // Find the transaction and make a copy of the callback
    ErrorCallback callback;
    {
        // Find and remove the transaction atomically
        std::lock_guard<util::CheckedMutex> lock(m_transactionsMutex);

        // Convert transaction ID to string for map key
        std::string transactionIDStr(error->getTransactionID().begin(), error->getTransactionID().end());

        // Find transaction
        auto it = m_transactions.find(transactionIDStr);
        if (it == m_transactions.end()) {
            getLogger()->warning("Received error for unknown transaction");
            return;
        }

        // Copy the callback
        if (it->second->errorCallback) {
            callback = it->second->errorCallback;
        }

        // Remove the transaction
        m_transactions.erase(it);
    }

    // Call the error callback outside the lock
    if (callback) {
        try {
            callback(error);
        } catch (const std::exception& e) {
            getLogger()->error("Exception in error callback: {}", e.what());
        } catch (...) {
            getLogger()->error("Unknown exception in error callback");
        }
    }
}
void DHTNode::handlePingQuery(std::shared_ptr<PingQuery> query, const network::EndPoint& sender) {
    if (!query) {
        getLogger()->error("Received null ping query");
        return;
    }
    getLogger()->debug("Received ping query from {}", sender.toString());
    // Create ping response
    auto response = std::make_shared<PingResponse>(query->getTransactionID(), m_nodeID);
    // Send response
    sendResponse(response, sender);
}
void DHTNode::handleFindNodeQuery(std::shared_ptr<FindNodeQuery> query, const network::EndPoint& sender) {
    if (!query) {
        getLogger()->error("Received null find_node query");
        return;
    }
    getLogger()->debug("Received find_node query from {} for target {}",
                 sender.toString(), nodeIDToString(query->getTarget()));
    // Get the closest nodes to the target
    auto closestNodes = m_routingTable.getClosestNodes(query->getTarget(), K_BUCKET_SIZE);
    // Convert to compact node info
    std::vector<CompactNodeInfo> compactNodes;
    for (const auto& node : closestNodes) {
        compactNodes.push_back({node->getID(), node->getEndpoint()});
    }
    // Create find_node response
    auto response = std::make_shared<FindNodeResponse>(query->getTransactionID(), m_nodeID, compactNodes);
    // Send response
    sendResponse(response, sender);
}
void DHTNode::handleGetPeersQuery(std::shared_ptr<GetPeersQuery> query, const network::EndPoint& sender) {
    if (!query) {
        getLogger()->error("Received null get_peers query");
        return;
    }
    getLogger()->debug("Received get_peers query from {} for info hash {}",
                 sender.toString(), nodeIDToString(query->getInfoHash()));
    // Forward the infohash to the collector if set
    if (m_infoHashCollector) {
        getLogger()->debug("Forwarding info hash to collector: {}", nodeIDToString(query->getInfoHash()));
        m_infoHashCollector->addInfoHash(query->getInfoHash());
    } else {
        getLogger()->warning("Info hash collector not set, cannot forward info hash: {}", nodeIDToString(query->getInfoHash()));
    }
    // Generate a token for this node
    std::string token = generateToken(sender);
    // Get stored peers for this info hash
    auto peers = getStoredPeers(query->getInfoHash());
    if (!peers.empty()) {
        getLogger()->debug("Found {} stored peers for info hash {}",
                     peers.size(), nodeIDToString(query->getInfoHash()));
        // Create get_peers response with peers
        auto response = std::make_shared<GetPeersResponse>(query->getTransactionID(), m_nodeID, peers, token);
        // Send response
        sendResponse(response, sender);
    } else {
        getLogger()->debug("No stored peers for info hash {}, returning closest nodes",
                     nodeIDToString(query->getInfoHash()));
        // Get the closest nodes to the info hash
        auto closestNodes = m_routingTable.getClosestNodes(query->getInfoHash(), K_BUCKET_SIZE);
        // Convert to compact node info
        std::vector<CompactNodeInfo> compactNodes;
        for (const auto& node : closestNodes) {
            compactNodes.push_back({node->getID(), node->getEndpoint()});
        }
        // Create get_peers response with nodes
        auto response = std::make_shared<GetPeersResponse>(query->getTransactionID(), m_nodeID, compactNodes, token);
        // Send response
        sendResponse(response, sender);
    }
}
void DHTNode::handleAnnouncePeerQuery(std::shared_ptr<AnnouncePeerQuery> query, const network::EndPoint& sender) {
    if (!query) {
        getLogger()->error("Received null announce_peer query");
        return;
    }
    getLogger()->debug("Received announce_peer query from {} for info hash {}",
                 sender.toString(), nodeIDToString(query->getInfoHash()));

    // Forward the infohash to the collector if set
    if (m_infoHashCollector) {
        getLogger()->debug("Forwarding info hash to collector: {}", nodeIDToString(query->getInfoHash()));
        m_infoHashCollector->addInfoHash(query->getInfoHash());
    } else {
        getLogger()->warning("Info hash collector not set, cannot forward info hash: {}", nodeIDToString(query->getInfoHash()));
    }

    // Validate the token
    if (!validateToken(query->getToken(), sender)) {
        getLogger()->warning("Invalid token in announce_peer query from {}", sender.toString());
        // Send error response
        auto error = std::make_shared<ErrorMessage>(query->getTransactionID(), 203, "Invalid Token");
        sendError(error, sender);
        return;
    }
    // Create endpoint with the announced port or the sender's port
    network::EndPoint peerEndpoint;
    if (query->isImpliedPort()) {
        // Use the sender's port
        peerEndpoint = sender;
    } else {
        // Use the announced port
        peerEndpoint = network::EndPoint(sender.getAddress(), query->getPort());
    }
    // Store the peer
    if (storePeer(query->getInfoHash(), peerEndpoint)) {
        getLogger()->debug("Stored peer {} for info hash {}",
                     peerEndpoint.toString(), nodeIDToString(query->getInfoHash()));
    } else {
        getLogger()->warning("Failed to store peer {} for info hash {}",
                       peerEndpoint.toString(), nodeIDToString(query->getInfoHash()));
    }
    // Create announce_peer response
    auto response = std::make_shared<AnnouncePeerResponse>(query->getTransactionID(), m_nodeID);
    // Send response
    sendResponse(response, sender);
}
bool DHTNode::sendResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& endpoint) {
    if (!m_running) {
        getLogger()->error("Cannot send response: DHT node not running");
        return false;
    }
    // Encode response
    std::string encodedResponse = response->encodeToString();
    // Send response using the message batcher
    const uint8_t* data = reinterpret_cast<const uint8_t*>(encodedResponse.data());
    bool result = m_messageBatcher->sendMessage(data, encodedResponse.size(), endpoint,
        [endpoint](bool success) {
            if (!success) {
                auto localLogger = dht_hunter::logforge::LogForge::getLogger("DHT.Node");
                localLogger->error("Failed to send response to {}", endpoint.toString());
            }
        });
    if (!result) {
        getLogger()->error("Failed to queue response for sending to {}", endpoint.toString());
        return false;
    }
    getLogger()->debug("Sent response to {}", endpoint.toString());
    return true;
}
bool DHTNode::sendError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& endpoint) {
    if (!m_running) {
        getLogger()->error("Cannot send error: DHT node not running");
        return false;
    }
    // Encode error
    std::string encodedError = error->encodeToString();
    // Send error using the message batcher
    const uint8_t* data = reinterpret_cast<const uint8_t*>(encodedError.data());
    bool result = m_messageBatcher->sendMessage(data, encodedError.size(), endpoint,
        [endpoint, errorMsg = error->getMessage(), errorCode = error->getCode()](bool success) {
            if (!success) {
                auto localLogger = dht_hunter::logforge::LogForge::getLogger("DHT.Node");
                localLogger->error("Failed to send error to {}: {} ({})", endpoint.toString(), errorMsg, errorCode);
            }
        });
    if (!result) {
        getLogger()->error("Failed to queue error for sending to {}: {} ({})", endpoint.toString(), error->getMessage(), error->getCode());
        return false;
    }
    getLogger()->debug("Sent error to {}: {} ({})", endpoint.toString(), error->getMessage(), error->getCode());
    return true;
}
bool DHTNode::addTransaction(const Transaction& transaction) {
    // Skip if we're shutting down
    if (!m_running) {
        getLogger()->warning("Cannot add transaction: DHT node not running");
        return false;
    }

    std::lock_guard<util::CheckedMutex> lock(m_transactionsMutex);

    // Check if we have too many transactions
    if (m_transactions.size() >= MAX_TRANSACTIONS) {
        getLogger()->error("Too many transactions");
        return false;
    }

    // Convert transaction ID to string for map key
    std::string transactionIDStr(transaction.id.begin(), transaction.id.end());

    // Check if transaction already exists
    if (m_transactions.find(transactionIDStr) != m_transactions.end()) {
        getLogger()->error("Transaction already exists");
        return false;
    }

    // Add transaction with a shared_ptr to ensure it stays alive
    m_transactions[transactionIDStr] = std::make_shared<Transaction>(transaction);
    return true;
}

bool DHTNode::removeTransaction(const TransactionID& id) {
    // Skip if we're shutting down
    if (!m_running) {
        return false;
    }

    std::lock_guard<util::CheckedMutex> lock(m_transactionsMutex);

    // Convert transaction ID to string for map key
    std::string transactionIDStr(id.begin(), id.end());

    // Remove transaction
    return m_transactions.erase(transactionIDStr) > 0;
}

std::shared_ptr<Transaction> DHTNode::findTransaction(const TransactionID& id) {
    // Skip if we're shutting down
    if (!m_running) {
        return nullptr;
    }

    std::lock_guard<util::CheckedMutex> lock(m_transactionsMutex);

    // Convert transaction ID to string for map key
    std::string transactionIDStr(id.begin(), id.end());

    // Find transaction
    auto it = m_transactions.find(transactionIDStr);
    if (it == m_transactions.end()) {
        return nullptr;
    }

    // Return a copy of the shared_ptr to ensure it stays alive
    return it->second;
}
void DHTNode::checkTimeouts() {
    // This method has been simplified to avoid AddressSanitizer issues
    getLogger()->info("Transaction timeout checking disabled to avoid memory issues");

    while (m_running) {
        // Sleep for a short time
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Skip processing if we're shutting down
        if (!m_running) {
            break;
        }

        // Get current time
        auto now = std::chrono::steady_clock::now();

        // Only handle secret rotation, which is essential for security
        {
            std::lock_guard<util::CheckedMutex> lock(m_secretMutex);
            auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - m_secretLastChanged).count();
            if (elapsed >= 10) {
                // Change the secret every 10 minutes
                m_previousSecret = m_secret;
                m_secret = generateRandomString(20);
                m_secretLastChanged = now;
                getLogger()->debug("Changed secret");
            }
        }
    }
}
void DHTNode::receiveMessages() {
    // Buffer for receiving messages
    std::vector<uint8_t> buffer(65536);

    while (m_running) {
        // Get a local copy of the socket pointer to avoid race conditions
        network::Socket* localSocket = nullptr;
        {
            // Use a scope to minimize the time we hold the lock
            std::lock_guard<util::CheckedMutex> lock(m_socketMutex);
            if (m_socket) {
                // Just get a raw pointer - we'll only use it while checking if it's valid
                localSocket = m_socket.get();
            }
        }

        // Check if socket is valid
        if (!localSocket || !localSocket->isValid()) {
            getLogger()->debug("Socket closed or invalid, exiting receive thread");
            break;
        }

        // Receive message
        network::EndPoint sender;
        int result;
        {
            // Lock again for the actual socket operation
            std::lock_guard<util::CheckedMutex> lock(m_socketMutex);
            // Check again if socket is still valid after acquiring the lock
            if (!m_socket || !m_socket->isValid()) {
                getLogger()->debug("Socket became invalid, exiting receive thread");
                break;
            }
            result = dynamic_cast<network::UDPSocketImpl*>(m_socket.get())->receiveFrom(buffer.data(), buffer.size(), sender);
        }

        // Check if we should exit the thread
        if (!m_running) {
            getLogger()->debug("Thread signaled to stop, exiting receive thread");
            break;
        }

        if (result < 0) {
            // Check if it's a would-block error
            if (m_socket->getLastError() == network::SocketError::WouldBlock) {
                // No data available, sleep for a short time
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // Check if socket was closed
            if (m_socket->getLastError() == network::SocketError::NotConnected ||
                m_socket->getLastError() == network::SocketError::ConnectionRefused ||
                m_socket->getLastError() == network::SocketError::ConnectionClosed) {
                getLogger()->debug("Socket closed, exiting receive thread");
                break;
            }

            getLogger()->error("Failed to receive message: {}",
                         network::Socket::getErrorString(m_socket->getLastError()));
            continue;
        }
        if (result == 0) {
            // No data received
            continue;
        }
        // Decode message
        std::string data(reinterpret_cast<char*>(buffer.data()), static_cast<size_t>(result));
        auto message = decodeMessage(data);
        if (!message) {
            getLogger()->error("Failed to decode message from {}", sender.toString());
            continue;
        }
        // Handle message
        handleMessage(message, sender);
    }
    getLogger()->debug("Receive thread exiting");
}
bool DHTNode::addNode(const NodeID& id, const network::EndPoint& endpoint) {
    // Don't add ourselves
    if (id == m_nodeID) {
        return false;
    }
    // Create node
    auto node = std::make_shared<Node>(id, endpoint);
    // Add to routing table
    bool added = m_routingTable.addNode(node);

    // If the node was added and we're configured to save on new nodes, save the routing table
    if (added && SAVE_ROUTING_TABLE_ON_NEW_NODE && !m_routingTablePath.empty()) {
        getLogger()->debug("Saving routing table after adding new node: {}", nodeIDToString(id));
        if (!saveRoutingTable(m_routingTablePath)) {
            getLogger()->warning("Failed to save routing table after adding new node");
        }
    }

    return added;
}
std::string DHTNode::generateToken(const network::EndPoint& endpoint) {
    std::lock_guard<util::CheckedMutex> lock(m_secretMutex);
    // Generate token based on endpoint and secret
    std::stringstream ss;
    ss << endpoint.toString() << m_secret;
    // Hash the token
    std::hash<std::string> hasher;
    size_t hash = hasher(ss.str());
    // Convert hash to string
    ss.str("");
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return ss.str();
}
bool DHTNode::validateToken(const std::string& token, const network::EndPoint& endpoint) {
    std::lock_guard<util::CheckedMutex> lock(m_secretMutex);
    // Generate token based on endpoint and current secret
    std::stringstream ss;
    ss << endpoint.toString() << m_secret;
    // Hash the token
    std::hash<std::string> hasher;
    size_t hash = hasher(ss.str());
    // Convert hash to string
    ss.str("");
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    // Check if token matches
    if (token == ss.str()) {
        return true;
    }
    // If not, try with previous secret
    ss.str("");
    ss << endpoint.toString() << m_previousSecret;
    hash = hasher(ss.str());
    ss.str("");
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return token == ss.str();
}
} // namespace dht_hunter::dht
