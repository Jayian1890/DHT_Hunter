#pragma once

#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/dht/core/dht_types.hpp"
#include "dht_hunter/dht/network/message.hpp"
#include "dht_hunter/dht/network/query_message.hpp"
#include "dht_hunter/dht/network/response_message.hpp"
#include "dht_hunter/dht/network/error_message.hpp"
#include "dht_hunter/event/logger.hpp"
#include <unordered_map>
#include <functional>
#include <mutex>
#include <chrono>
#include <thread>
#include <atomic>
#include <random>

namespace dht_hunter::dht {

/**
 * @brief Callback for a transaction response
 */
using TransactionResponseCallback = std::function<void(std::shared_ptr<ResponseMessage>, const network::EndPoint&)>;

/**
 * @brief Callback for a transaction error
 */
using TransactionErrorCallback = std::function<void(std::shared_ptr<ErrorMessage>, const network::EndPoint&)>;

/**
 * @brief Callback for a transaction timeout
 */
using TransactionTimeoutCallback = std::function<void()>;

/**
 * @brief A transaction
 */
struct Transaction {
    std::string id;
    std::shared_ptr<QueryMessage> query;
    network::EndPoint endpoint;
    std::chrono::steady_clock::time_point timestamp;
    TransactionResponseCallback responseCallback;
    TransactionErrorCallback errorCallback;
    TransactionTimeoutCallback timeoutCallback;
    
    Transaction(const std::string& id,
               std::shared_ptr<QueryMessage> query,
               const network::EndPoint& endpoint,
               TransactionResponseCallback responseCallback,
               TransactionErrorCallback errorCallback,
               TransactionTimeoutCallback timeoutCallback)
        : id(id),
          query(query),
          endpoint(endpoint),
          timestamp(std::chrono::steady_clock::now()),
          responseCallback(responseCallback),
          errorCallback(errorCallback),
          timeoutCallback(timeoutCallback) {}
};

/**
 * @brief Manages DHT transactions
 */
class TransactionManager {
public:
    /**
     * @brief Constructs a transaction manager
     * @param config The DHT configuration
     */
    explicit TransactionManager(const DHTConfig& config);

    /**
     * @brief Destructor
     */
    ~TransactionManager();

    /**
     * @brief Starts the transaction manager
     * @return True if the transaction manager was started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stops the transaction manager
     */
    void stop();

    /**
     * @brief Checks if the transaction manager is running
     * @return True if the transaction manager is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Creates a transaction
     * @param query The query message
     * @param endpoint The endpoint
     * @param responseCallback The response callback
     * @param errorCallback The error callback
     * @param timeoutCallback The timeout callback
     * @return The transaction ID
     */
    std::string createTransaction(std::shared_ptr<QueryMessage> query,
                                 const network::EndPoint& endpoint,
                                 TransactionResponseCallback responseCallback,
                                 TransactionErrorCallback errorCallback,
                                 TransactionTimeoutCallback timeoutCallback);

    /**
     * @brief Handles a response message
     * @param response The response message
     * @param sender The sender
     */
    void handleResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender);

    /**
     * @brief Handles an error message
     * @param error The error message
     * @param sender The sender
     */
    void handleError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender);

    /**
     * @brief Gets the number of active transactions
     * @return The number of active transactions
     */
    size_t getTransactionCount() const;

private:
    /**
     * @brief Generates a random transaction ID
     * @return The transaction ID
     */
    std::string generateTransactionID();

    /**
     * @brief Checks for timed out transactions
     */
    void checkTimeouts();

    /**
     * @brief Checks for timed out transactions periodically
     */
    void checkTimeoutsPeriodically();

    DHTConfig m_config;
    std::unordered_map<std::string, Transaction> m_transactions;
    std::atomic<bool> m_running;
    std::thread m_timeoutThread;
    mutable std::mutex m_mutex;
    std::mt19937 m_rng;
    event::Logger m_logger;
};

} // namespace dht_hunter::dht
