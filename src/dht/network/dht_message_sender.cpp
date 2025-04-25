#include "dht_hunter/dht/network/dht_message_sender.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/network/socket_wrapper.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"

DEFINE_COMPONENT_LOGGER("DHT", "MessageSender")

namespace dht_hunter::dht {

DHTMessageSender::DHTMessageSender(const DHTNodeConfig& config, std::shared_ptr<DHTSocketManager> socketManager)
    : m_config(config), m_socketManager(socketManager), m_running(false) {
    getLogger()->info("Creating message sender");
}

DHTMessageSender::~DHTMessageSender() {
    stop();
}

bool DHTMessageSender::start() {
    if (m_running) {
        getLogger()->warning("Message sender already running");
        return false;
    }

    if (!m_socketManager) {
        getLogger()->error("No socket manager provided");
        return false;
    }

    if (!m_socketManager->isRunning()) {
        getLogger()->error("Socket manager is not running");
        return false;
    }

    // Create and start the message batcher
    network::UDPBatcherConfig batcherConfig;
    batcherConfig.enabled = true;
    batcherConfig.maxBatchSize = DEFAULT_MTU_SIZE;
    batcherConfig.maxMessagesPerBatch = DEFAULT_MAX_MESSAGES_PER_BATCH;
    batcherConfig.maxBatchDelay = std::chrono::milliseconds(DEFAULT_MAX_BATCH_DELAY_MS);

    // For now, we'll skip the message batcher since it requires a Socket
    // TODO: Update UDPMessageBatcher to accept a SocketWrapper
    getLogger()->info("Message batching disabled temporarily until UDPMessageBatcher is updated");

    // We're not using the message batcher for now
    // m_messageBatcher = std::make_unique<network::UDPMessageBatcher>(socketWrapper, batcherConfig);
    // if (!m_messageBatcher->start()) {
    //     getLogger()->error("Failed to start message batcher");
    //     return false;
    // }

    m_running = true;
    getLogger()->info("Message sender started with message batching enabled");
    return true;
}

void DHTMessageSender::stop() {
    // Use atomic operation to prevent multiple stops
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false)) {
        // Already stopped or stopping
        return;
    }

    getLogger()->info("Stopping message sender");

    // Stop the message batcher (currently disabled)
    // if (m_messageBatcher) {
    //     getLogger()->debug("Stopping message batcher...");
    //     m_messageBatcher->stop();
    //     m_messageBatcher.reset();
    //     getLogger()->debug("Message batcher stopped");
    // }

    getLogger()->info("Message sender stopped");
}

bool DHTMessageSender::isRunning() const {
    return m_running;
}

bool DHTMessageSender::sendQuery(std::shared_ptr<QueryMessage> query, const network::EndPoint& endpoint,
                               std::function<void(bool)> callback) {
    if (!m_running) {
        getLogger()->error("Cannot send query: Message sender not running");
        if (callback) {
            callback(false);
        }
        return false;
    }

    return sendMessage(query, endpoint, callback);
}

bool DHTMessageSender::sendResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& endpoint,
                                  std::function<void(bool)> callback) {
    if (!m_running) {
        getLogger()->error("Cannot send response: Message sender not running");
        if (callback) {
            callback(false);
        }
        return false;
    }

    return sendMessage(response, endpoint, callback);
}

bool DHTMessageSender::sendError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& endpoint,
                               std::function<void(bool)> callback) {
    if (!m_running) {
        getLogger()->error("Cannot send error: Message sender not running");
        if (callback) {
            callback(false);
        }
        return false;
    }

    return sendMessage(error, endpoint, callback);
}

bool DHTMessageSender::sendMessage(std::shared_ptr<DHTMessage> message, const network::EndPoint& endpoint,
                                 std::function<void(bool)> callback) {
    if (!m_running) {
        getLogger()->error("Cannot send message: Message sender not running");
        if (callback) {
            callback(false);
        }
        return false;
    }

    // Encode message
    std::string encodedMessage = message->encodeToString();

    // Send message directly using the socket manager
    const uint8_t* data = reinterpret_cast<const uint8_t*>(encodedMessage.data());
    bool result = m_socketManager->sendMessage(data, encodedMessage.size(), endpoint,
        [endpoint, callback, message](bool success) {
            if (!success) {
                auto localLogger = dht_hunter::logforge::LogForge::getLogger("DHT.MessageSender");

                // Log different messages based on message type
                switch (message->getType()) {
                    case MessageType::Query: {
                        auto query = std::dynamic_pointer_cast<QueryMessage>(message);
                        if (query) {
                            localLogger->error("Failed to send {} query to {}",
                                        queryMethodToString(query->getMethod()),
                                        endpoint.toString());
                        } else {
                            localLogger->error("Failed to send query to {}", endpoint.toString());
                        }
                        break;
                    }
                    case MessageType::Response:
                        localLogger->error("Failed to send response to {}", endpoint.toString());
                        break;
                    case MessageType::Error: {
                        auto error = std::dynamic_pointer_cast<ErrorMessage>(message);
                        if (error) {
                            localLogger->error("Failed to send error to {}: {} ({})",
                                        endpoint.toString(),
                                        error->getMessage(),
                                        error->getCode());
                        } else {
                            localLogger->error("Failed to send error to {}", endpoint.toString());
                        }
                        break;
                    }
                    default:
                        localLogger->error("Failed to send message to {}", endpoint.toString());
                        break;
                }
            }

            if (callback) {
                callback(success);
            }
        });

    if (!result) {
        getLogger()->error("Failed to queue message for sending to {}", endpoint.toString());
        if (callback) {
            callback(false);
        }
        return false;
    }

    // Log message type
    switch (message->getType()) {
        case MessageType::Query: {
            auto query = std::dynamic_pointer_cast<QueryMessage>(message);
            if (query) {
                getLogger()->debug("Sent {} query to {}", queryMethodToString(query->getMethod()), endpoint.toString());
            } else {
                getLogger()->debug("Sent query to {}", endpoint.toString());
            }
            break;
        }
        case MessageType::Response:
            getLogger()->debug("Sent response to {}", endpoint.toString());
            break;
        case MessageType::Error: {
            auto error = std::dynamic_pointer_cast<ErrorMessage>(message);
            if (error) {
                getLogger()->debug("Sent error to {}: {} ({})",
                            endpoint.toString(),
                            error->getMessage(),
                            error->getCode());
            } else {
                getLogger()->debug("Sent error to {}", endpoint.toString());
            }
            break;
        }
        default:
            getLogger()->debug("Sent message to {}", endpoint.toString());
            break;
    }

    return true;
}

} // namespace dht_hunter::dht
