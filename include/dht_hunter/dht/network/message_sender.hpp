#pragma once

#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/dht/network/socket_manager.hpp"
#include "dht_hunter/dht/network/message.hpp"
#include "dht_hunter/dht/network/query_message.hpp"
#include "dht_hunter/dht/network/response_message.hpp"
#include "dht_hunter/dht/network/error_message.hpp"
#include "dht_hunter/event/logger.hpp"
#include <memory>
#include <atomic>
#include <mutex>

namespace dht_hunter::dht {

/**
 * @brief Sends DHT messages
 */
class MessageSender {
public:
    /**
     * @brief Constructs a message sender
     * @param config The DHT configuration
     * @param socketManager The socket manager
     */
    MessageSender(const DHTConfig& config, std::shared_ptr<SocketManager> socketManager);

    /**
     * @brief Destructor
     */
    ~MessageSender();

    /**
     * @brief Starts the message sender
     * @return True if the message sender was started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stops the message sender
     */
    void stop();

    /**
     * @brief Checks if the message sender is running
     * @return True if the message sender is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Sends a query message
     * @param query The query message
     * @param endpoint The endpoint to send to
     * @return True if the message was sent successfully, false otherwise
     */
    bool sendQuery(std::shared_ptr<QueryMessage> query, const network::EndPoint& endpoint);

    /**
     * @brief Sends a response message
     * @param response The response message
     * @param endpoint The endpoint to send to
     * @return True if the message was sent successfully, false otherwise
     */
    bool sendResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& endpoint);

    /**
     * @brief Sends an error message
     * @param error The error message
     * @param endpoint The endpoint to send to
     * @return True if the message was sent successfully, false otherwise
     */
    bool sendError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& endpoint);

private:
    /**
     * @brief Sends a message
     * @param message The message
     * @param endpoint The endpoint to send to
     * @return True if the message was sent successfully, false otherwise
     */
    bool sendMessage(std::shared_ptr<Message> message, const network::EndPoint& endpoint);

    DHTConfig m_config;
    std::shared_ptr<SocketManager> m_socketManager;
    std::atomic<bool> m_running;
    std::mutex m_mutex;
    event::Logger m_logger;
};

} // namespace dht_hunter::dht
