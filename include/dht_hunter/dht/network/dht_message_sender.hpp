#pragma once

#include "dht_hunter/dht/core/dht_node_config.hpp"
#include "dht_hunter/dht/network/dht_socket_manager.hpp"
#include "dht_hunter/dht/network/dht_message.hpp"
#include "dht_hunter/dht/network/dht_query_message.hpp"
#include "dht_hunter/dht/network/dht_response_message.hpp"
#include "dht_hunter/dht/network/dht_error_message.hpp"
#include "dht_hunter/dht/transactions/dht_transaction_types.hpp"
#include "dht_hunter/network/udp_message_batcher.hpp"
#include <memory>
#include <functional>

namespace dht_hunter::dht {

/**
 * @brief Responsible for encoding and sending DHT messages
 */
class DHTMessageSender {
public:
    /**
     * @brief Constructs a message sender
     * @param config The DHT node configuration
     * @param socketManager The socket manager to use for sending messages
     */
    DHTMessageSender(const DHTNodeConfig& config, std::shared_ptr<DHTSocketManager> socketManager);

    /**
     * @brief Destructor
     */
    ~DHTMessageSender();

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
     * @param query The query message to send
     * @param endpoint The endpoint to send the query to
     * @param callback The callback to call when the query is sent
     * @return True if the query was queued for sending, false otherwise
     */
    bool sendQuery(std::shared_ptr<QueryMessage> query, const network::EndPoint& endpoint,
                  std::function<void(bool)> callback = nullptr);

    /**
     * @brief Sends a response message
     * @param response The response message to send
     * @param endpoint The endpoint to send the response to
     * @param callback The callback to call when the response is sent
     * @return True if the response was queued for sending, false otherwise
     */
    bool sendResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& endpoint,
                     std::function<void(bool)> callback = nullptr);

    /**
     * @brief Sends an error message
     * @param error The error message to send
     * @param endpoint The endpoint to send the error to
     * @param callback The callback to call when the error is sent
     * @return True if the error was queued for sending, false otherwise
     */
    bool sendError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& endpoint,
                  std::function<void(bool)> callback = nullptr);

private:
    /**
     * @brief Sends a message
     * @param message The message to send
     * @param endpoint The endpoint to send the message to
     * @param callback The callback to call when the message is sent
     * @return True if the message was queued for sending, false otherwise
     */
    bool sendMessage(std::shared_ptr<DHTMessage> message, const network::EndPoint& endpoint,
                    std::function<void(bool)> callback = nullptr);

    DHTNodeConfig m_config;
    std::shared_ptr<DHTSocketManager> m_socketManager;
    std::unique_ptr<network::UDPMessageBatcher> m_messageBatcher;
    std::atomic<bool> m_running{false};
};

} // namespace dht_hunter::dht
