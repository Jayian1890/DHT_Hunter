#include "dht_hunter/dht/dht_node.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/network/platform/socket_impl.hpp"

#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <algorithm>

namespace dht_hunter::dht {

namespace {
    static auto logger = dht_hunter::logforge::LogForge::getLogger("DHT.Node");

    // Generate a random string of the specified length
    std::string generateRandomString(size_t length) {
        static const char charset[] = "0123456789"
                                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                     "abcdefghijklmnopqrstuvwxyz";

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);

        std::string result;
        result.reserve(length);

        for (size_t i = 0; i < length; ++i) {
            result += charset[dist(gen)];
        }

        return result;
    }
}

DHTNode::DHTNode(uint16_t port, const NodeID& nodeID)
    : m_nodeID(nodeID), m_port(port), m_routingTable(nodeID), m_running(false),
      m_secret(generateRandomString(20)), m_secretLastChanged(std::chrono::steady_clock::now()) {

    logger->info("Creating DHT node with ID: {}", nodeIDToString(m_nodeID));
}

DHTNode::~DHTNode() {
    stop();
}

bool DHTNode::start() {
    if (m_running) {
        logger->warning("DHT node already running");
        return false;
    }

    // Create UDP socket
    m_socket = network::SocketFactory::createUDPSocket();

    // Bind to port
    if (!m_socket->bind(network::EndPoint(network::NetworkAddress::any(), m_port))) {
        logger->error("Failed to bind socket to port {}: {}", m_port,
                     network::Socket::getErrorString(m_socket->getLastError()));
        return false;
    }

    // Set non-blocking mode
    if (!m_socket->setNonBlocking(true)) {
        logger->error("Failed to set non-blocking mode: {}",
                     network::Socket::getErrorString(m_socket->getLastError()));
        return false;
    }

    logger->info("DHT node started on port {}", m_port);

    // Start threads
    m_running = true;
    m_receiveThread = std::thread(&DHTNode::receiveMessages, this);
    m_timeoutThread = std::thread(&DHTNode::checkTimeouts, this);

    return true;
}

void DHTNode::stop() {
    if (!m_running) {
        return;
    }

    logger->info("Stopping DHT node");

    // Stop threads
    m_running = false;

    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }

    if (m_timeoutThread.joinable()) {
        m_timeoutThread.join();
    }

    // Close socket
    m_socket->close();
    m_socket.reset();

    // Clear transactions
    {
        std::lock_guard<std::mutex> lock(m_transactionsMutex);
        m_transactions.clear();
    }

    logger->info("DHT node stopped");
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

bool DHTNode::bootstrap(const network::EndPoint& endpoint) {
    if (!m_running) {
        logger->error("Cannot bootstrap: DHT node not running");
        return false;
    }

    logger->info("Bootstrapping DHT node using {}", endpoint.toString());

    // Send a find_node query for our own ID
    return findNode(m_nodeID, endpoint,
                   [this, endpoint](std::shared_ptr<ResponseMessage> response) {
                       // Add the responding node to our routing table
                       addNode(response->getNodeID(), endpoint);

                       // Add nodes from the response to our routing table
                       auto findNodeResponse = std::dynamic_pointer_cast<FindNodeResponse>(response);
                       if (findNodeResponse) {
                           for (const auto& node : findNodeResponse->getNodes()) {
                               addNode(node.id, node.endpoint);
                           }

                           logger->info("Bootstrap successful, added {} nodes",
                                       findNodeResponse->getNodes().size() + 1);
                       }
                   },
                   [](std::shared_ptr<ErrorMessage> error) {
                       logger->error("Bootstrap failed: {} ({})",
                                    error->getMessage(), error->getCode());
                   },
                   []() {
                       logger->error("Bootstrap timed out");
                   });
}

bool DHTNode::bootstrap(const std::vector<network::EndPoint>& endpoints) {
    if (endpoints.empty()) {
        logger->error("Cannot bootstrap: No endpoints provided");
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
        logger->error("Cannot ping: DHT node not running");
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
        logger->error("Cannot find_node: DHT node not running");
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
        logger->error("Cannot get_peers: DHT node not running");
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
        logger->error("Cannot announce_peer: DHT node not running");
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
        logger->error("Cannot send query: DHT node not running");
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
        logger->error("Failed to add transaction");
        return false;
    }

    // Send query
    const uint8_t* data = reinterpret_cast<const uint8_t*>(encodedQuery.data());
    int result = dynamic_cast<network::UDPSocketImpl*>(m_socket.get())->sendTo(data, encodedQuery.size(), endpoint);

    if (result < 0) {
        logger->error("Failed to send query: {}",
                     network::Socket::getErrorString(m_socket->getLastError()));
        removeTransaction(transaction.id);
        return false;
    }

    logger->debug("Sent {} query to {}", queryMethodToString(query->getMethod()), endpoint.toString());
    return true;
}

void DHTNode::handleMessage(std::shared_ptr<Message> message, const network::EndPoint& sender) {
    if (!message) {
        logger->error("Received null message");
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
            logger->error("Received message with unknown type");
            break;
    }
}

void DHTNode::handleQuery(std::shared_ptr<QueryMessage> query, const network::EndPoint& sender) {
    if (!query) {
        logger->error("Received null query");
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
            logger->error("Received query with unknown method");

            // Send error response
            auto error = std::make_shared<ErrorMessage>(query->getTransactionID(), 204, "Method Unknown");
            sendError(error, sender);
            break;
    }
}

void DHTNode::handleResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender) {
    if (!response) {
        logger->error("Received null response");
        return;
    }

    // Add the sender to our routing table
    addNode(response->getNodeID(), sender);

    // Find the transaction
    auto transaction = findTransaction(response->getTransactionID());
    if (!transaction) {
        logger->warning("Received response for unknown transaction");
        return;
    }

    // Remove the transaction
    removeTransaction(response->getTransactionID());

    // Call the response callback
    if (transaction->responseCallback) {
        transaction->responseCallback(response);
    }
}

void DHTNode::handleError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& /* sender */) {
    if (!error) {
        logger->error("Received null error");
        return;
    }

    // Find the transaction
    auto transaction = findTransaction(error->getTransactionID());
    if (!transaction) {
        logger->warning("Received error for unknown transaction");
        return;
    }

    // Remove the transaction
    removeTransaction(error->getTransactionID());

    // Call the error callback
    if (transaction->errorCallback) {
        transaction->errorCallback(error);
    }
}

void DHTNode::handlePingQuery(std::shared_ptr<PingQuery> query, const network::EndPoint& sender) {
    if (!query) {
        logger->error("Received null ping query");
        return;
    }

    logger->debug("Received ping query from {}", sender.toString());

    // Create ping response
    auto response = std::make_shared<PingResponse>(query->getTransactionID(), m_nodeID);

    // Send response
    sendResponse(response, sender);
}

void DHTNode::handleFindNodeQuery(std::shared_ptr<FindNodeQuery> query, const network::EndPoint& sender) {
    if (!query) {
        logger->error("Received null find_node query");
        return;
    }

    logger->debug("Received find_node query from {} for target {}",
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
        logger->error("Received null get_peers query");
        return;
    }

    logger->debug("Received get_peers query from {} for info hash {}",
                 sender.toString(), nodeIDToString(query->getInfoHash()));

    // Generate a token for this node
    std::string token = generateToken(sender);

    // For now, we don't store peers, so we just return the closest nodes
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

void DHTNode::handleAnnouncePeerQuery(std::shared_ptr<AnnouncePeerQuery> query, const network::EndPoint& sender) {
    if (!query) {
        logger->error("Received null announce_peer query");
        return;
    }

    logger->debug("Received announce_peer query from {} for info hash {}",
                 sender.toString(), nodeIDToString(query->getInfoHash()));

    // Validate the token
    if (!validateToken(query->getToken(), sender)) {
        logger->warning("Invalid token in announce_peer query from {}", sender.toString());

        // Send error response
        auto error = std::make_shared<ErrorMessage>(query->getTransactionID(), 203, "Invalid Token");
        sendError(error, sender);
        return;
    }

    // For now, we don't store peers, so we just acknowledge the announce

    // Create announce_peer response
    auto response = std::make_shared<AnnouncePeerResponse>(query->getTransactionID(), m_nodeID);

    // Send response
    sendResponse(response, sender);
}

bool DHTNode::sendResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& endpoint) {
    if (!m_running) {
        logger->error("Cannot send response: DHT node not running");
        return false;
    }

    // Encode response
    std::string encodedResponse = response->encodeToString();

    // Send response
    const uint8_t* data = reinterpret_cast<const uint8_t*>(encodedResponse.data());
    int result = dynamic_cast<network::UDPSocketImpl*>(m_socket.get())->sendTo(data, encodedResponse.size(), endpoint);

    if (result < 0) {
        logger->error("Failed to send response: {}",
                     network::Socket::getErrorString(m_socket->getLastError()));
        return false;
    }

    logger->debug("Sent response to {}", endpoint.toString());
    return true;
}

bool DHTNode::sendError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& endpoint) {
    if (!m_running) {
        logger->error("Cannot send error: DHT node not running");
        return false;
    }

    // Encode error
    std::string encodedError = error->encodeToString();

    // Send error
    const uint8_t* data = reinterpret_cast<const uint8_t*>(encodedError.data());
    int result = dynamic_cast<network::UDPSocketImpl*>(m_socket.get())->sendTo(data, encodedError.size(), endpoint);

    if (result < 0) {
        logger->error("Failed to send error: {}",
                     network::Socket::getErrorString(m_socket->getLastError()));
        return false;
    }

    logger->debug("Sent error to {}: {} ({})", endpoint.toString(), error->getMessage(), error->getCode());
    return true;
}

bool DHTNode::addTransaction(const Transaction& transaction) {
    std::lock_guard<std::mutex> lock(m_transactionsMutex);

    // Check if we have too many transactions
    if (m_transactions.size() >= MAX_TRANSACTIONS) {
        logger->error("Too many transactions");
        return false;
    }

    // Convert transaction ID to string for map key
    std::string transactionIDStr(transaction.id.begin(), transaction.id.end());

    // Check if transaction already exists
    if (m_transactions.find(transactionIDStr) != m_transactions.end()) {
        logger->error("Transaction already exists");
        return false;
    }

    // Add transaction
    m_transactions[transactionIDStr] = std::make_shared<Transaction>(transaction);

    return true;
}

bool DHTNode::removeTransaction(const TransactionID& id) {
    std::lock_guard<std::mutex> lock(m_transactionsMutex);

    // Convert transaction ID to string for map key
    std::string transactionIDStr(id.begin(), id.end());

    // Remove transaction
    return m_transactions.erase(transactionIDStr) > 0;
}

std::shared_ptr<Transaction> DHTNode::findTransaction(const TransactionID& id) {
    std::lock_guard<std::mutex> lock(m_transactionsMutex);

    // Convert transaction ID to string for map key
    std::string transactionIDStr(id.begin(), id.end());

    // Find transaction
    auto it = m_transactions.find(transactionIDStr);
    if (it == m_transactions.end()) {
        return nullptr;
    }

    return it->second;
}

void DHTNode::checkTimeouts() {
    while (m_running) {
        // Sleep for a short time
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Get current time
        auto now = std::chrono::steady_clock::now();

        // Check for timed out transactions
        std::vector<TransactionID> timedOutTransactions;

        {
            std::lock_guard<std::mutex> lock(m_transactionsMutex);

            for (const auto& [idStr, transaction] : m_transactions) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - transaction->timestamp).count();

                if (elapsed >= TRANSACTION_TIMEOUT) {
                    timedOutTransactions.push_back(transaction->id);
                }
            }
        }

        // Handle timed out transactions
        for (const auto& id : timedOutTransactions) {
            auto transaction = findTransaction(id);
            if (transaction) {
                // Remove transaction
                removeTransaction(id);

                // Call timeout callback
                if (transaction->timeoutCallback) {
                    transaction->timeoutCallback();
                }
            }
        }

        // Check if we need to change the secret
        {
            std::lock_guard<std::mutex> lock(m_secretMutex);

            auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - m_secretLastChanged).count();

            if (elapsed >= 10) {
                // Change the secret every 10 minutes
                m_previousSecret = m_secret;
                m_secret = generateRandomString(20);
                m_secretLastChanged = now;

                logger->debug("Changed secret");
            }
        }
    }
}

void DHTNode::receiveMessages() {
    // Buffer for receiving messages
    std::vector<uint8_t> buffer(65536);

    while (m_running) {
        // Receive message
        network::EndPoint sender;
        int result = dynamic_cast<network::UDPSocketImpl*>(m_socket.get())->receiveFrom(buffer.data(), buffer.size(), sender);

        if (result < 0) {
            // Check if it's a would-block error
            if (m_socket->getLastError() == network::SocketError::WouldBlock) {
                // No data available, sleep for a short time
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            logger->error("Failed to receive message: {}",
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
            logger->error("Failed to decode message from {}", sender.toString());
            continue;
        }

        // Handle message
        handleMessage(message, sender);
    }
}

bool DHTNode::addNode(const NodeID& id, const network::EndPoint& endpoint) {
    // Don't add ourselves
    if (id == m_nodeID) {
        return false;
    }

    // Create node
    auto node = std::make_shared<Node>(id, endpoint);

    // Add to routing table
    return m_routingTable.addNode(node);
}

std::string DHTNode::generateToken(const network::EndPoint& endpoint) {
    std::lock_guard<std::mutex> lock(m_secretMutex);

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
    std::lock_guard<std::mutex> lock(m_secretMutex);

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
