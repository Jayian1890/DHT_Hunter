#pragma once

#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/dht/network/socket_manager.hpp"
#include "dht_hunter/dht/network/message.hpp"
#include "dht_hunter/dht/network/query_message.hpp"
#include "dht_hunter/dht/network/response_message.hpp"
#include "dht_hunter/dht/network/error_message.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"
#include <memory>
#include <atomic>
#include <mutex>

namespace dht_hunter::dht {

/**
 * @brief Sends DHT messages (Singleton)
 */
class MessageSender {
public:
    /**
     * @brief Gets the singleton instance of the message sender
     * @param config The DHT configuration (only used if instance doesn't exist yet)
     * @param socketManager The socket manager (only used if instance doesn't exist yet)
     * @return The singleton instance
     */
    static std::shared_ptr<MessageSender> getInstance(
        const DHTConfig& config,
        std::shared_ptr<SocketManager> socketManager);

    /**
     * @brief Destructor
     */
    ~MessageSender();

    /**
     * @brief Delete copy constructor and assignment operator
     */
    MessageSender(const MessageSender&) = delete;
    MessageSender& operator=(const MessageSender&) = delete;
    MessageSender(MessageSender&&) = delete;
    MessageSender& operator=(MessageSender&&) = delete;

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

    /**
     * @brief Private constructor for singleton pattern
     */
    MessageSender(const DHTConfig& config, std::shared_ptr<SocketManager> socketManager);

    // Static instance for singleton pattern
    static std::shared_ptr<MessageSender> s_instance;
    static std::mutex s_instanceMutex;

    DHTConfig m_config;
    std::shared_ptr<SocketManager> m_socketManager;
    std::shared_ptr<unified_event::EventBus> m_eventBus;
    std::atomic<bool> m_running;
    std::mutex m_mutex;    // Logger removed
};

} // namespace dht_hunter::dht
