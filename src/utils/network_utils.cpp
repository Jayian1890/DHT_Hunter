#include "utils/network_utils.hpp"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sstream>
#include <iomanip>

namespace dht_hunter::utils::network {

//=============================================================================
// UDPSocket Implementation
//=============================================================================

class UDPSocket::Impl {
public:
    Impl() : m_socket(-1), m_running(false) {}

    ~Impl() {
        close();
    }

    bool bind(uint16_t port) {
        // Create socket if it doesn't exist
        if (m_socket < 0) {
            m_socket = socket(AF_INET, SOCK_DGRAM, 0);
            if (m_socket < 0) {
                return false;
            }

            // Set non-blocking mode
            int flags = fcntl(m_socket, F_GETFL, 0);
            if (flags < 0) {
                close();
                return false;
            }
            if (fcntl(m_socket, F_SETFL, flags | O_NONBLOCK) < 0) {
                close();
                return false;
            }
        }

        // Bind to the specified port
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (::bind(m_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close();
            return false;
        }

        return true;
    }

    ssize_t sendTo(const void* data, size_t size, const std::string& address, uint16_t port) {
        if (m_socket < 0) {
            return -1;
        }

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
            return -1;
        }

        return sendto(m_socket, data, size, 0, (struct sockaddr*)&addr, sizeof(addr));
    }

    ssize_t receiveFrom(void* buffer, size_t size, std::string& address, uint16_t& port) {
        if (m_socket < 0) {
            return -1;
        }

        struct sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        std::memset(&addr, 0, addrLen);

        ssize_t received = recvfrom(m_socket, buffer, size, 0, (struct sockaddr*)&addr, &addrLen);
        if (received > 0) {
            char addrStr[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &addr.sin_addr, addrStr, INET_ADDRSTRLEN)) {
                address = addrStr;
                port = ntohs(addr.sin_port);
            } else {
                address = "";
                port = 0;
            }
        }

        return received;
    }

    bool startReceiveLoop() {
        if (m_socket < 0) {
            return false;
        }

        if (m_running) {
            return true;
        }

        m_running = true;
        m_receiveThread = std::thread([this]() {
            std::vector<uint8_t> buffer(DEFAULT_MTU_SIZE);
            std::string address;
            uint16_t port;

            while (m_running) {
                ssize_t received = receiveFrom(buffer.data(), buffer.size(), address, port);
                if (received > 0) {
                    if (m_receiveCallback) {
                        // Resize the buffer to the actual received size
                        buffer.resize(received);
                        m_receiveCallback(buffer, address, port);
                        // Restore the buffer size for the next receive
                        buffer.resize(DEFAULT_MTU_SIZE);
                    }
                } else if (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    // Error occurred
                    break;
                }

                // Sleep a bit to avoid busy-waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });

        return true;
    }

    void stopReceiveLoop() {
        m_running = false;
        if (m_receiveThread.joinable()) {
            m_receiveThread.join();
        }
    }

    void setReceiveCallback(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback) {
        m_receiveCallback = callback;
    }

    bool isValid() const {
        return m_socket >= 0;
    }

    void close() {
        stopReceiveLoop();
        if (m_socket >= 0) {
            ::close(m_socket);
            m_socket = -1;
        }
    }

private:
    int m_socket;
    std::atomic<bool> m_running;
    std::thread m_receiveThread;
    std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> m_receiveCallback;
};

UDPSocket::UDPSocket() : m_impl(std::make_unique<Impl>()) {
}

UDPSocket::~UDPSocket() {
}

bool UDPSocket::bind(uint16_t port) {
    return m_impl->bind(port);
}

ssize_t UDPSocket::sendTo(const void* data, size_t size, const std::string& address, uint16_t port) {
    return m_impl->sendTo(data, size, address, port);
}

ssize_t UDPSocket::receiveFrom(void* buffer, size_t size, std::string& address, uint16_t& port) {
    return m_impl->receiveFrom(buffer, size, address, port);
}

bool UDPSocket::startReceiveLoop() {
    return m_impl->startReceiveLoop();
}

void UDPSocket::stopReceiveLoop() {
    m_impl->stopReceiveLoop();
}

void UDPSocket::setReceiveCallback(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback) {
    m_impl->setReceiveCallback(callback);
}

bool UDPSocket::isValid() const {
    return m_impl->isValid();
}

void UDPSocket::close() {
    m_impl->close();
}

//=============================================================================
// UDPServer Implementation
//=============================================================================

UDPServer::UDPServer() : m_running(false) {
}

UDPServer::~UDPServer() {
    stop();
}

bool UDPServer::start(uint16_t port) {
    if (m_running) {
        return true;
    }

    if (!m_socket.bind(port)) {
        return false;
    }

    m_running = true;
    return m_socket.startReceiveLoop();
}

bool UDPServer::start() {
    return start(DEFAULT_UDP_PORT);
}

void UDPServer::stop() {
    if (!m_running) {
        return;
    }

    m_socket.stopReceiveLoop();
    m_socket.close();
    m_running = false;
}

bool UDPServer::isRunning() const {
    return m_running;
}

ssize_t UDPServer::sendTo(const std::vector<uint8_t>& data, const std::string& address, uint16_t port) {
    return m_socket.sendTo(data.data(), data.size(), address, port);
}

void UDPServer::setMessageHandler(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback) {
    m_socket.setReceiveCallback(callback);
}

//=============================================================================
// UDPClient Implementation
//=============================================================================

UDPClient::UDPClient() : m_running(false) {
}

UDPClient::~UDPClient() {
    stop();
}

bool UDPClient::start() {
    if (m_running) {
        return true;
    }

    // Bind to any available port
    if (!m_socket.bind(0)) {
        return false;
    }

    m_running = true;
    return m_socket.startReceiveLoop();
}

void UDPClient::stop() {
    if (!m_running) {
        return;
    }

    m_socket.stopReceiveLoop();
    m_socket.close();
    m_running = false;
}

bool UDPClient::isRunning() const {
    return m_running;
}

ssize_t UDPClient::send(const std::vector<uint8_t>& data, const std::string& address, uint16_t port) {
    return m_socket.sendTo(data.data(), data.size(), address, port);
}

void UDPClient::setMessageHandler(std::function<void(const std::vector<uint8_t>&, const std::string&, uint16_t)> callback) {
    m_socket.setReceiveCallback(callback);
}

//=============================================================================
// SocketManager Implementation
//=============================================================================

// Initialize static members
std::shared_ptr<SocketManager> SocketManager::s_instance = nullptr;
std::recursive_mutex SocketManager::s_instanceMutex;

std::shared_ptr<SocketManager> SocketManager::getInstance(const dht_core::DHTConfig& config) {
    std::lock_guard<std::recursive_mutex> lock(s_instanceMutex);

    if (!s_instance) {
        s_instance = std::shared_ptr<SocketManager>(new SocketManager(config));
    }

    return s_instance;
}

SocketManager::SocketManager(const dht_core::DHTConfig& config)
    : m_config(config), m_port(config.getPort()), m_running(false), m_socket(std::make_unique<UDPSocket>()) {
}

SocketManager::~SocketManager() {
    stop();
}

bool SocketManager::start(std::function<void(const uint8_t*, size_t, const types::EndPoint&)> receiveCallback) {
    if (m_running) {
        return true;
    }

    if (!m_socket->bind(m_port)) {
        return false;
    }

    m_socket->setReceiveCallback([this, receiveCallback](const std::vector<uint8_t>& data, const std::string& address, uint16_t port) {
        types::EndPoint endpoint(address, port);
        receiveCallback(data.data(), data.size(), endpoint);
    });

    if (!m_socket->startReceiveLoop()) {
        return false;
    }

    m_running = true;
    return true;
}

void SocketManager::stop() {
    if (!m_running) {
        return;
    }

    m_socket->stopReceiveLoop();
    m_socket->close();
    m_running = false;
}

bool SocketManager::isRunning() const {
    return m_running;
}

uint16_t SocketManager::getPort() const {
    return m_port;
}

ssize_t SocketManager::sendTo(const void* data, size_t size, const types::EndPoint& endpoint) {
    return m_socket->sendTo(data, size, endpoint.getAddress(), endpoint.getPort());
}

//=============================================================================
// Message Implementation
//=============================================================================

Message::Message(MessageType type) : m_type(type) {
}

MessageType Message::getType() const {
    return m_type;
}

//=============================================================================
// QueryMessage Implementation
//=============================================================================

QueryMessage::QueryMessage(QueryType queryType)
    : Message(MessageType::QUERY), m_queryType(queryType) {
}

QueryType QueryMessage::getQueryType() const {
    return m_queryType;
}

void QueryMessage::setTransactionID(const std::string& transactionID) {
    m_transactionID = transactionID;
}

const std::string& QueryMessage::getTransactionID() const {
    return m_transactionID;
}

void QueryMessage::setNodeID(const types::NodeID& nodeID) {
    m_nodeID = nodeID;
}

const types::NodeID& QueryMessage::getNodeID() const {
    return m_nodeID;
}

std::shared_ptr<common::BencodeValue> QueryMessage::encode() const {
    // Implementation would go here
    return nullptr;
}

bool QueryMessage::decode(const std::shared_ptr<common::BencodeValue>& dict) {
    // Implementation would go here
    return false;
}

//=============================================================================
// ResponseMessage Implementation
//=============================================================================

ResponseMessage::ResponseMessage() : Message(MessageType::RESPONSE) {
}

void ResponseMessage::setTransactionID(const std::string& transactionID) {
    m_transactionID = transactionID;
}

const std::string& ResponseMessage::getTransactionID() const {
    return m_transactionID;
}

void ResponseMessage::setNodeID(const types::NodeID& nodeID) {
    m_nodeID = nodeID;
}

const types::NodeID& ResponseMessage::getNodeID() const {
    return m_nodeID;
}

std::shared_ptr<common::BencodeValue> ResponseMessage::encode() const {
    // Implementation would go here
    return nullptr;
}

bool ResponseMessage::decode(const std::shared_ptr<common::BencodeValue>& dict) {
    // Implementation would go here
    return false;
}

//=============================================================================
// ErrorMessage Implementation
//=============================================================================

ErrorMessage::ErrorMessage() : Message(MessageType::ERROR), m_errorCode(0) {
}

void ErrorMessage::setTransactionID(const std::string& transactionID) {
    m_transactionID = transactionID;
}

const std::string& ErrorMessage::getTransactionID() const {
    return m_transactionID;
}

void ErrorMessage::setErrorCode(int code) {
    m_errorCode = code;
}

int ErrorMessage::getErrorCode() const {
    return m_errorCode;
}

void ErrorMessage::setErrorMessage(const std::string& message) {
    m_errorMessage = message;
}

const std::string& ErrorMessage::getErrorMessage() const {
    return m_errorMessage;
}

std::shared_ptr<common::BencodeValue> ErrorMessage::encode() const {
    // Implementation would go here
    return nullptr;
}

bool ErrorMessage::decode(const std::shared_ptr<common::BencodeValue>& dict) {
    // Implementation would go here
    return false;
}

//=============================================================================
// MessageSender Implementation
//=============================================================================

// Initialize static members
std::shared_ptr<MessageSender> MessageSender::s_instance = nullptr;
std::mutex MessageSender::s_instanceMutex;

std::shared_ptr<MessageSender> MessageSender::getInstance(
    const dht_core::DHTConfig& config,
    const types::NodeID& nodeID,
    std::shared_ptr<SocketManager> socketManager) {
    std::lock_guard<std::mutex> lock(s_instanceMutex);

    if (!s_instance) {
        s_instance = std::shared_ptr<MessageSender>(new MessageSender(config, nodeID, socketManager));
    }

    return s_instance;
}

MessageSender::MessageSender(
    const dht_core::DHTConfig& config,
    const types::NodeID& nodeID,
    std::shared_ptr<SocketManager> socketManager)
    : m_config(config), m_nodeID(nodeID), m_socketManager(socketManager) {
}

MessageSender::~MessageSender() {
}

bool MessageSender::sendQuery(std::shared_ptr<QueryMessage> query, const types::EndPoint& endpoint) {
    return sendMessage(query, endpoint);
}

bool MessageSender::sendResponse(std::shared_ptr<ResponseMessage> response, const types::EndPoint& endpoint) {
    return sendMessage(response, endpoint);
}

bool MessageSender::sendError(std::shared_ptr<ErrorMessage> error, const types::EndPoint& endpoint) {
    return sendMessage(error, endpoint);
}

bool MessageSender::sendMessage(std::shared_ptr<Message> message, const types::EndPoint& endpoint) {
    // Implementation would go here
    return false;
}

//=============================================================================
// MessageHandler Implementation
//=============================================================================

// Initialize static members
std::shared_ptr<MessageHandler> MessageHandler::s_instance = nullptr;
std::mutex MessageHandler::s_instanceMutex;

std::shared_ptr<MessageHandler> MessageHandler::getInstance(
    const dht_core::DHTConfig& config,
    const types::NodeID& nodeID,
    std::shared_ptr<MessageSender> messageSender,
    std::shared_ptr<dht_core::RoutingTable> routingTable) {
    std::lock_guard<std::mutex> lock(s_instanceMutex);

    if (!s_instance) {
        s_instance = std::shared_ptr<MessageHandler>(new MessageHandler(config, nodeID, messageSender, routingTable));
    }

    return s_instance;
}

MessageHandler::MessageHandler(
    const dht_core::DHTConfig& config,
    const types::NodeID& nodeID,
    std::shared_ptr<MessageSender> messageSender,
    std::shared_ptr<dht_core::RoutingTable> routingTable)
    : m_config(config), m_nodeID(nodeID), m_messageSender(messageSender), m_routingTable(routingTable) {
}

MessageHandler::~MessageHandler() {
}

void MessageHandler::handleMessage(const uint8_t* data, size_t size, const types::EndPoint& endpoint) {
    // Implementation would go here
}

void MessageHandler::handleQuery(std::shared_ptr<QueryMessage> query, const types::EndPoint& endpoint) {
    // Implementation would go here
}

void MessageHandler::handleResponse(std::shared_ptr<ResponseMessage> response, const types::EndPoint& endpoint) {
    // Implementation would go here
}

void MessageHandler::handleError(std::shared_ptr<ErrorMessage> error, const types::EndPoint& endpoint) {
    // Implementation would go here
}

//=============================================================================
// Transaction Implementation
//=============================================================================

Transaction::Transaction(
    const std::string& id,
    std::shared_ptr<QueryMessage> query,
    const types::EndPoint& endpoint,
    TransactionResponseCallback responseCallback,
    TransactionErrorCallback errorCallback,
    TransactionTimeoutCallback timeoutCallback)
    : m_id(id),
      m_query(query),
      m_endpoint(endpoint),
      m_timestamp(std::chrono::steady_clock::now()),
      m_responseCallback(responseCallback),
      m_errorCallback(errorCallback),
      m_timeoutCallback(timeoutCallback) {
}

const std::string& Transaction::getID() const {
    return m_id;
}

std::shared_ptr<QueryMessage> Transaction::getQuery() const {
    return m_query;
}

const types::EndPoint& Transaction::getEndpoint() const {
    return m_endpoint;
}

const std::chrono::steady_clock::time_point& Transaction::getTimestamp() const {
    return m_timestamp;
}

void Transaction::handleResponse(std::shared_ptr<ResponseMessage> response, const types::EndPoint& endpoint) {
    if (m_responseCallback) {
        m_responseCallback(response, endpoint);
    }
}

void Transaction::handleError(std::shared_ptr<ErrorMessage> error, const types::EndPoint& endpoint) {
    if (m_errorCallback) {
        m_errorCallback(error, endpoint);
    }
}

void Transaction::handleTimeout() {
    if (m_timeoutCallback) {
        m_timeoutCallback();
    }
}

//=============================================================================
// TransactionManager Implementation
//=============================================================================

// Initialize static members
std::shared_ptr<TransactionManager> TransactionManager::s_instance = nullptr;
std::mutex TransactionManager::s_instanceMutex;

std::shared_ptr<TransactionManager> TransactionManager::getInstance(
    const dht_core::DHTConfig& config,
    const types::NodeID& nodeID) {
    std::lock_guard<std::mutex> lock(s_instanceMutex);

    if (!s_instance) {
        s_instance = std::shared_ptr<TransactionManager>(new TransactionManager(config, nodeID));
    }

    return s_instance;
}

TransactionManager::TransactionManager(const dht_core::DHTConfig& config, const types::NodeID& nodeID)
    : m_config(config), m_nodeID(nodeID), m_running(false), m_gen(m_rd()) {
    m_running = true;
    m_timeoutThread = std::thread(&TransactionManager::checkTimeoutsPeriodically, this);
}

TransactionManager::~TransactionManager() {
    m_running = false;
    if (m_timeoutThread.joinable()) {
        m_timeoutThread.join();
    }
}

std::string TransactionManager::createTransaction(
    std::shared_ptr<QueryMessage> query,
    const types::EndPoint& endpoint,
    TransactionResponseCallback responseCallback,
    TransactionErrorCallback errorCallback,
    TransactionTimeoutCallback timeoutCallback) {
    std::string transactionID = generateTransactionID();
    query->setTransactionID(transactionID);

    std::lock_guard<std::mutex> lock(m_mutex);
    m_transactions[transactionID] = std::make_shared<Transaction>(
        transactionID, query, endpoint, responseCallback, errorCallback, timeoutCallback);

    return transactionID;
}

bool TransactionManager::handleResponse(std::shared_ptr<ResponseMessage> response, const types::EndPoint& endpoint) {
    std::string transactionID = response->getTransactionID();

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_transactions.find(transactionID);
    if (it != m_transactions.end()) {
        it->second->handleResponse(response, endpoint);
        m_transactions.erase(it);
        return true;
    }

    return false;
}

bool TransactionManager::handleError(std::shared_ptr<ErrorMessage> error, const types::EndPoint& endpoint) {
    std::string transactionID = error->getTransactionID();

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_transactions.find(transactionID);
    if (it != m_transactions.end()) {
        it->second->handleError(error, endpoint);
        m_transactions.erase(it);
        return true;
    }

    return false;
}

std::string TransactionManager::generateTransactionID() {
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
    uint32_t random = dist(m_gen);

    std::stringstream ss;
    ss << std::hex << std::setw(8) << std::setfill('0') << random;
    return ss.str();
}

void TransactionManager::checkTimeouts() {
    auto now = std::chrono::steady_clock::now();
    std::vector<std::string> timedOutTransactions;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& pair : m_transactions) {
            auto transaction = pair.second;
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - transaction->getTimestamp()).count();
            if (elapsed > m_config.getTransactionTimeout()) {
                timedOutTransactions.push_back(pair.first);
            }
        }

        for (const auto& id : timedOutTransactions) {
            auto it = m_transactions.find(id);
            if (it != m_transactions.end()) {
                it->second->handleTimeout();
                m_transactions.erase(it);
            }
        }
    }
}

void TransactionManager::checkTimeoutsPeriodically() {
    while (m_running) {
        checkTimeouts();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

} // namespace dht_hunter::utils::network
