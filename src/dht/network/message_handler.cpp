#include "dht_hunter/dht/network/message_handler.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"
#include "dht_hunter/dht/storage/token_manager.hpp"
#include "dht_hunter/dht/storage/peer_storage.hpp"
#include "dht_hunter/dht/transactions/transaction_manager.hpp"

namespace dht_hunter::dht {

// Initialize static members
std::shared_ptr<MessageHandler> MessageHandler::s_instance = nullptr;
std::mutex MessageHandler::s_instanceMutex;

std::shared_ptr<MessageHandler> MessageHandler::getInstance(
    const DHTConfig& config,
    const NodeID& nodeID,
    std::shared_ptr<MessageSender> messageSender,
    std::shared_ptr<dht::RoutingTable> routingTable,
    std::shared_ptr<TokenManager> tokenManager,
    std::shared_ptr<PeerStorage> peerStorage,
    std::shared_ptr<TransactionManager> transactionManager) {

    try {
        return utility::thread::withLock(s_instanceMutex, [&]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<MessageHandler>(new MessageHandler(
                    config, nodeID, messageSender, routingTable, tokenManager, peerStorage, transactionManager));
            }
            return s_instance;
        }, "MessageHandler::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.MessageHandler", e.what());
        return nullptr;
    }
}

MessageHandler::MessageHandler(const DHTConfig& config,
                             const NodeID& nodeID,
                             std::shared_ptr<MessageSender> messageSender,
                             std::shared_ptr<dht::RoutingTable> routingTable,
                             std::shared_ptr<TokenManager> tokenManager,
                             std::shared_ptr<PeerStorage> peerStorage,
                             std::shared_ptr<TransactionManager> transactionManager)
    : m_config(config),
      m_nodeID(nodeID),
      m_messageSender(messageSender),
      m_routingTable(routingTable),
      m_tokenManager(tokenManager),
      m_peerStorage(peerStorage),
      m_transactionManager(transactionManager),
      m_eventBus(unified_event::EventBus::getInstance()) {
}

MessageHandler::~MessageHandler() {
    // Clear the singleton instance
    try {
        utility::thread::withLock(s_instanceMutex, [this]() {
            if (s_instance.get() == this) {
                s_instance.reset();
            }
        }, "MessageHandler::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.MessageHandler", e.what());
    }
}

bool MessageHandler::start() {

    // Publish a system started event
    auto startedEvent = std::make_shared<unified_event::SystemStartedEvent>("DHT.MessageHandler");
    m_eventBus->publish(startedEvent);

    return true;
}

void MessageHandler::stop() {

    // Publish a system stopped event
    auto stoppedEvent = std::make_shared<unified_event::SystemStoppedEvent>("DHT.MessageHandler");
    m_eventBus->publish(stoppedEvent);
}

void MessageHandler::handleRawMessage(const uint8_t* data, size_t size, const network::EndPoint& sender) {
    if (!data || size == 0) {
        return;
    }

    // Decode the message
    std::shared_ptr<Message> message = Message::decode(data, size);

    if (!message) {
        return;
    }

    // Set the sender IP in responses
    if (message->getType() == MessageType::Response) {
        auto response = std::dynamic_pointer_cast<ResponseMessage>(message);
        if (response && response->getSenderIP().empty()) {
            // Only set the sender IP if it's not already set
            response->setSenderIP(sender.getAddress().toString());
        }
    }

    // Handle the message
    handleMessage(message, sender);
}

void MessageHandler::handleMessage(std::shared_ptr<Message> message, const network::EndPoint& sender) {
    if (!message) {
        return;
    }

    // Publish a message received event
    // Convert EndPoint to NetworkAddress
    network::NetworkAddress networkAddress = sender.getAddress();
    auto event = std::make_shared<unified_event::MessageReceivedEvent>("DHT.MessageHandler", message, networkAddress);
    m_eventBus->publish(event);

    // Update the routing table with the sender's node ID
    if (message->getNodeID()) {
        updateRoutingTable(message->getNodeID().value(), sender);
    }

    // Handle the message based on its type
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
            break;
    }
}

void MessageHandler::handleQuery(std::shared_ptr<QueryMessage> query, const network::EndPoint& sender) {
    if (!query) {
        return;
    }

    // Validate the query
    if (!query->getNodeID()) {

        // Send an error response
        if (m_messageSender) {
            auto error = ErrorMessage::createInvalidNodeIDError(query->getTransactionID(), "Query does not have a node ID");
            m_messageSender->sendError(error, sender);
        }

        return;
    }

    // Log the query
    std::string methodStr;
    switch (query->getMethod()) {
        case QueryMethod::Ping: methodStr = "ping"; break;
        case QueryMethod::FindNode: methodStr = "find_node"; break;
        case QueryMethod::GetPeers: methodStr = "get_peers"; break;
        case QueryMethod::AnnouncePeer: methodStr = "announce_peer"; break;
        default: methodStr = "unknown";
    }

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
            break;
    }
}

void MessageHandler::handleResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender) {
    if (!response) {
        return;
    }

    // Forward the response to the transaction manager
    if (m_transactionManager) {
        m_transactionManager->handleResponse(response, sender);
    } else {
    }
}

void MessageHandler::handleError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender) {
    if (!error) {
        return;
    }

    // Forward the error to the transaction manager
    if (m_transactionManager) {
        m_transactionManager->handleError(error, sender);
    } else {
    }
}

void MessageHandler::handlePingQuery(std::shared_ptr<PingQuery> query, const network::EndPoint& sender) {
    if (!query) {
        return;
    }

    // Create a ping response
    auto response = std::make_shared<PingResponse>(query->getTransactionID(), m_nodeID);

    // Send the response
    if (m_messageSender) {
        m_messageSender->sendResponse(response, sender);
    } else {
    }
}

void MessageHandler::handleFindNodeQuery(std::shared_ptr<FindNodeQuery> query, const network::EndPoint& sender) {
    if (!query) {
        return;
    }

    // Validate the target node ID
    if (!isValidNodeID(query->getTargetID())) {

        // Send an error response
        if (m_messageSender) {
            auto error = ErrorMessage::createInvalidNodeIDError(query->getTransactionID(), "Invalid target node ID");
            m_messageSender->sendError(error, sender);
        }

        return;
    }

    // Get the closest nodes to the target ID
    std::vector<std::shared_ptr<Node>> closestNodes;
    if (m_routingTable) {
        closestNodes = m_routingTable->getClosestNodes(query->getTargetID(), m_config.getKBucketSize());
    } else {
    }

    // Create a find_node response
    auto response = std::make_shared<FindNodeResponse>(query->getTransactionID(), m_nodeID, closestNodes);

    // Send the response
    if (m_messageSender) {
        m_messageSender->sendResponse(response, sender);
    } else {
    }
}

void MessageHandler::handleGetPeersQuery(std::shared_ptr<GetPeersQuery> query, const network::EndPoint& sender) {
    if (!query) {
        return;
    }

    // Validate the info hash
    if (!isValidInfoHash(query->getInfoHash())) {

        // Send an error response
        if (m_messageSender) {
            auto error = ErrorMessage::createInvalidInfoHashError(query->getTransactionID(), "Invalid info hash");
            m_messageSender->sendError(error, sender);
        }

        return;
    }

    // Generate a token for the sender
    std::string token;
    if (m_tokenManager) {
        token = m_tokenManager->generateToken(sender);
    } else {
        token = "dummy_token";
    }

    // Check if we have peers for the info hash
    std::vector<network::EndPoint> peers;
    if (m_peerStorage) {
        peers = m_peerStorage->getPeers(query->getInfoHash());
    } else {
    }

    // Create a get_peers response
    std::shared_ptr<GetPeersResponse> response;
    if (!peers.empty()) {
        // We have peers, return them
        response = std::make_shared<GetPeersResponse>(query->getTransactionID(), m_nodeID, peers, token);
    } else {
        // We don't have peers, return the closest nodes
        std::vector<std::shared_ptr<Node>> closestNodes;
        if (m_routingTable) {
            closestNodes = m_routingTable->getClosestNodes(NodeID(query->getInfoHash()), m_config.getKBucketSize());
        } else {
        }

        response = std::make_shared<GetPeersResponse>(query->getTransactionID(), m_nodeID, closestNodes, token);
    }

    // Send the response
    if (m_messageSender) {
        m_messageSender->sendResponse(response, sender);
    } else {
    }
}

void MessageHandler::handleAnnouncePeerQuery(std::shared_ptr<AnnouncePeerQuery> query, const network::EndPoint& sender) {
    if (!query) {
        return;
    }

    // Validate the info hash
    if (!isValidInfoHash(query->getInfoHash())) {

        // Send an error response
        if (m_messageSender) {
            auto error = ErrorMessage::createInvalidInfoHashError(query->getTransactionID(), "Invalid info hash");
            m_messageSender->sendError(error, sender);
        }

        return;
    }

    // Validate the port
    if (!query->isImpliedPort() && (query->getPort() == 0 || query->getPort() > 65535)) {

        // Send an error response
        if (m_messageSender) {
            auto error = ErrorMessage::createInvalidPortError(query->getTransactionID(), "Invalid port");
            m_messageSender->sendError(error, sender);
        }

        return;
    }

    // Validate the token
    if (!m_tokenManager || !m_tokenManager->verifyToken(query->getToken(), sender)) {

        // Send an error response
        if (m_messageSender) {
            auto error = ErrorMessage::createInvalidTokenError(query->getTransactionID(), "Invalid token");
            m_messageSender->sendError(error, sender);
        }

        return;
    }

    // Verify the token
    bool validToken = false;
    if (m_tokenManager) {
        validToken = m_tokenManager->verifyToken(query->getToken(), sender);
    } else {
        validToken = true;
    }

    if (!validToken) {

        // Send an error response
        auto error = std::make_shared<ErrorMessage>(query->getTransactionID(), ErrorCode::ProtocolError, "Invalid token");

        if (m_messageSender) {
            m_messageSender->sendError(error, sender);
        } else {
        }

        return;
    }

    // Store the peer
    uint16_t port = query->isImpliedPort() ? sender.getPort() : query->getPort();

    if (m_peerStorage) {
        network::NetworkAddress address = sender.getAddress();
        network::EndPoint endpoint(address, port);

        m_peerStorage->addPeer(query->getInfoHash(), endpoint);
    } else {
    }

    // Create an announce_peer response
    auto response = std::make_shared<AnnouncePeerResponse>(query->getTransactionID(), m_nodeID);

    // Send the response
    if (m_messageSender) {
        m_messageSender->sendResponse(response, sender);
    } else {
    }
}

void MessageHandler::updateRoutingTable(const NodeID& nodeID, const network::EndPoint& endpoint) {
    if (!m_routingTable) {
        return;
    }

    // Create a node
    auto node = std::make_shared<Node>(nodeID, endpoint);

    // Add the node to the routing table
    if (m_routingTable->addNode(node)) {
    }
}

} // namespace dht_hunter::dht
