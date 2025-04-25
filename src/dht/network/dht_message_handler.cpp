#include "dht_hunter/dht/network/dht_message_handler.hpp"
#include "dht_hunter/dht/storage/dht_token_manager.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include "dht_hunter/bencode/bencode.hpp"
#include "dht_hunter/crawler/infohash_collector.hpp"
#include <vector>

DEFINE_COMPONENT_LOGGER("DHT", "MessageHandler")

namespace dht_hunter::dht {

DHTMessageHandler::DHTMessageHandler(const DHTNodeConfig& config,
                                   const NodeID& nodeID,
                                   std::shared_ptr<DHTMessageSender> messageSender,
                                   std::shared_ptr<DHTRoutingManager> routingManager,
                                   std::shared_ptr<DHTTransactionManager> transactionManager,
                                   std::shared_ptr<DHTPeerStorage> peerStorage,
                                   std::shared_ptr<DHTTokenManager> tokenManager)
    : m_config(config),
      m_nodeID(nodeID),
      m_messageSender(messageSender),
      m_routingManager(routingManager),
      m_transactionManager(transactionManager),
      m_peerStorage(peerStorage),
      m_tokenManager(tokenManager) {
    getLogger()->info("Message handler created");
}

void DHTMessageHandler::setInfoHashCollector(std::shared_ptr<crawler::InfoHashCollector> collector) {
    std::lock_guard<util::CheckedMutex> lock(m_infoHashCollectorMutex);
    m_infoHashCollector = collector;
    getLogger()->info("InfoHash collector set");
}

std::shared_ptr<crawler::InfoHashCollector> DHTMessageHandler::getInfoHashCollector() const {
    std::lock_guard lock(m_infoHashCollectorMutex);
    return m_infoHashCollector;
}

void DHTMessageHandler::handleRawMessage(const uint8_t* data, size_t size, const network::EndPoint& sender) {
    if (!data || size == 0) {
        getLogger()->error("Invalid message data");
        return;
    }

    // Try to decode the message
    try {
        // Convert the raw data to a string
        std::string messageStr(reinterpret_cast<const char*>(data), size);

        // Try to decode the message
        bencode::BencodeParser parser;
        auto root = parser.parse(messageStr);

        // Create a message from the decoded data
        auto message = DHTMessage::decode(root.getDictionary());
        if (!message) {
            getLogger()->error("Failed to create message from bencode data");
            return;
        }

        // Handle the decoded message
        handleMessage(message, sender);
    } catch (const std::exception& e) {
        getLogger()->error("Exception while decoding message: {}", e.what());
    } catch (...) {
        getLogger()->error("Unknown exception while decoding message");
    }
}

void DHTMessageHandler::handleMessage(std::shared_ptr<DHTMessage> message, const network::EndPoint& sender) {
    if (!message) {
        getLogger()->error("Invalid message");
        return;
    }

    // Update the routing table with the sender's node ID
    if (message->getNodeID().has_value()) {
        updateRoutingTable(message->getNodeID().value(), sender);
    }

    // Handle the message based on its type
    switch (message->getType()) {
        case MessageType::Query: {
            auto query = std::dynamic_pointer_cast<QueryMessage>(message);
            if (query) {
                handleQuery(query, sender);
            } else {
                getLogger()->error("Failed to cast message to QueryMessage");
            }
            break;
        }
        case MessageType::Response: {
            auto response = std::dynamic_pointer_cast<ResponseMessage>(message);
            if (response) {
                handleResponse(response, sender);
            } else {
                getLogger()->error("Failed to cast message to ResponseMessage");
            }
            break;
        }
        case MessageType::Error: {
            auto error = std::dynamic_pointer_cast<ErrorMessage>(message);
            if (error) {
                handleError(error, sender);
            } else {
                getLogger()->error("Failed to cast message to ErrorMessage");
            }
            break;
        }
        default:
            getLogger()->error("Unknown message type: {}", static_cast<int>(message->getType()));
            break;
    }
}

void DHTMessageHandler::handleQuery(std::shared_ptr<QueryMessage> query, const network::EndPoint& sender) {
    if (!query) {
        getLogger()->error("Invalid query");
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
    getLogger()->debug("Received {} query from {}", methodStr, sender.toString());

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
            getLogger()->error("Unknown query method: {}", static_cast<int>(query->getMethod()));
            break;
    }
}

void DHTMessageHandler::handleResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender) {
    if (!response) {
        getLogger()->error("Invalid response");
        return;
    }

    // Log the response
    getLogger()->debug("Received response from {}", sender.toString());

    // Find the transaction
    if (!m_transactionManager) {
        getLogger()->error("No transaction manager available");
        return;
    }

    // Handle the response using the transaction manager
    if (!m_transactionManager->handleResponse(response, sender)) {
        getLogger()->warning("Failed to handle response from {}: Transaction {} not found",
                      sender.toString(), response->getTransactionID());
    }
}

void DHTMessageHandler::handleError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender) {
    if (!error) {
        getLogger()->error("Invalid error");
        return;
    }

    // Log the error
    getLogger()->warning("Received error from {}: {} ({})", sender.toString(), error->getMessage(), error->getCode());

    // Find the transaction
    if (!m_transactionManager) {
        getLogger()->error("No transaction manager available");
        return;
    }

    // Handle the error using the transaction manager
    if (!m_transactionManager->handleError(error, sender)) {
        getLogger()->warning("Failed to handle error from {}: Transaction {} not found",
                      sender.toString(), error->getTransactionID());
    }
}

void DHTMessageHandler::handlePingQuery(std::shared_ptr<PingQuery> query, const network::EndPoint& sender) {
    if (!query) {
        getLogger()->error("Invalid ping query");
        return;
    }

    // Create a ping response
    auto response = std::make_shared<PingResponse>(query->getTransactionID(), m_nodeID);

    // Send the response
    if (m_messageSender) {
        m_messageSender->sendResponse(response, sender);
    } else {
        getLogger()->error("No message sender available");
    }
}

void DHTMessageHandler::handleFindNodeQuery(std::shared_ptr<FindNodeQuery> query, const network::EndPoint& sender) {
    if (!query) {
        getLogger()->error("Invalid find_node query");
        return;
    }

    // TODO: Implement find_node query handling
    // For now, just send an empty response
    auto response = std::make_shared<FindNodeResponse>(query->getTransactionID(), m_nodeID);

    // Send the response
    if (m_messageSender) {
        m_messageSender->sendResponse(response, sender);
    } else {
        getLogger()->error("No message sender available");
    }
}

void DHTMessageHandler::handleGetPeersQuery(std::shared_ptr<GetPeersQuery> query, const network::EndPoint& sender) {
    if (!query) {
        getLogger()->error("Invalid get_peers query");
        return;
    }

    // Collect the info hash if we have a collector
    {
        std::lock_guard<util::CheckedMutex> lock(m_infoHashCollectorMutex);
        if (m_infoHashCollector) {
            m_infoHashCollector->collectInfoHash(query->getInfoHash(), sender);
        }
    }

    // TODO: Implement get_peers query handling
    // For now, just send an empty response
    auto response = std::make_shared<GetPeersResponse>(query->getTransactionID(), m_nodeID);

    // Generate a token for the sender
    if (m_tokenManager) {
        std::string token = m_tokenManager->generateToken(sender);
        response->setToken(token);
    }

    // Send the response
    if (m_messageSender) {
        m_messageSender->sendResponse(response, sender);
    } else {
        getLogger()->error("No message sender available");
    }
}

void DHTMessageHandler::handleAnnouncePeerQuery(std::shared_ptr<AnnouncePeerQuery> query, const network::EndPoint& sender) {
    if (!query) {
        getLogger()->error("Invalid announce_peer query");
        return;
    }

    // Validate the token
    bool tokenValid = false;
    if (m_tokenManager) {
        tokenValid = m_tokenManager->validateToken(query->getToken(), sender);
    }

    if (!tokenValid) {
        getLogger()->warning("Invalid token in announce_peer query from {}", sender.toString());

        // Send an error response
        auto error = std::make_shared<ErrorMessage>(query->getTransactionID(), 203, "Invalid token");
        error->setNodeID(m_nodeID);

        if (m_messageSender) {
            m_messageSender->sendError(error, sender);
        } else {
            getLogger()->error("No message sender available");
        }

        return;
    }

    // Store the peer
    if (m_peerStorage) {
        // Create an endpoint with the announced port
        network::EndPoint peerEndpoint(sender.getAddress(), query->getPort());

        // Store the peer
        // TODO: Implement peer storage
        getLogger()->debug("Storing peer {} for info hash {}",
                     peerEndpoint.toString(),
                     infoHashToString(query->getInfoHash()));
    }

    // Collect the info hash if we have a collector
    {
        std::lock_guard<util::CheckedMutex> lock(m_infoHashCollectorMutex);
        if (m_infoHashCollector) {
            m_infoHashCollector->collectInfoHash(query->getInfoHash(), sender);
        }
    }

    // Send a response
    auto response = std::make_shared<AnnouncePeerResponse>(query->getTransactionID(), m_nodeID);

    if (m_messageSender) {
        m_messageSender->sendResponse(response, sender);
    } else {
        getLogger()->error("No message sender available");
    }
}

void DHTMessageHandler::updateRoutingTable(const NodeID& nodeID, const network::EndPoint& endpoint) {
    // Don't add ourselves to the routing table
    if (nodeID == m_nodeID) {
        return;
    }

    // TODO: Implement routing table update
    // For now, just log the node
    getLogger()->debug("Would add node {} at {} to routing table", nodeIDToString(nodeID), endpoint.toString());
}

} // namespace dht_hunter::dht
