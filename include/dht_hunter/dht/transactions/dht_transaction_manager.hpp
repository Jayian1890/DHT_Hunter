#pragma once

#include "dht_hunter/dht/core/dht_node_config.hpp"
#include "dht_hunter/dht/network/dht_message.hpp"
#include "dht_hunter/dht/network/dht_query_message.hpp"
#include "dht_hunter/dht/network/dht_response_message.hpp"
#include "dht_hunter/dht/network/dht_error_message.hpp"
#include "dht_hunter/dht/core/dht_types.hpp"
#include "dht_hunter/dht/transactions/dht_transaction_types.hpp"
#include "dht_hunter/util/mutex_utils.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <queue>
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>

namespace dht_hunter::dht {

// Transaction callbacks are now defined in dht_transaction_types.hpp

/**
 * @brief Represents a DHT transaction
 */
class Transaction {
public:
    /**
     * @brief Constructs a transaction
     * @param id The transaction ID
     * @param query The query message
     * @param endpoint The endpoint the query was sent to
     * @param responseCallback The callback to call when a response is received
     * @param errorCallback The callback to call when an error is received
     * @param timeoutCallback The callback to call when the transaction times out
     */
    Transaction(const std::string& id,
               std::shared_ptr<QueryMessage> query,
               const network::EndPoint& endpoint,
               ResponseCallback responseCallback = nullptr,
               ErrorCallback errorCallback = nullptr,
               TimeoutCallback timeoutCallback = nullptr);

    /**
     * @brief Gets the transaction ID
     * @return The transaction ID
     */
    const std::string& getID() const;

    /**
     * @brief Gets the query message
     * @return The query message
     */
    std::shared_ptr<QueryMessage> getQuery() const;

    /**
     * @brief Gets the endpoint the query was sent to
     * @return The endpoint
     */
    const network::EndPoint& getEndpoint() const;

    /**
     * @brief Gets the creation time
     * @return The creation time
     */
    std::chrono::steady_clock::time_point getCreationTime() const;

    /**
     * @brief Checks if the transaction has timed out
     * @param timeout The timeout in seconds
     * @return True if the transaction has timed out, false otherwise
     */
    bool hasTimedOut(int timeout) const;

    /**
     * @brief Handles a response message
     * @param response The response message
     */
    void handleResponse(std::shared_ptr<ResponseMessage> response);

    /**
     * @brief Handles an error message
     * @param error The error message
     */
    void handleError(std::shared_ptr<ErrorMessage> error);

    /**
     * @brief Handles a timeout
     */
    void handleTimeout();

private:
    std::string m_id;
    std::shared_ptr<QueryMessage> m_query;
    network::EndPoint m_endpoint;
    std::chrono::steady_clock::time_point m_creationTime;
    ResponseCallback m_responseCallback;
    ErrorCallback m_errorCallback;
    TimeoutCallback m_timeoutCallback;
};

/**
 * @brief Manages DHT transactions
 */
class DHTTransactionManager {
public:
    /**
     * @brief Constructs a transaction manager
     * @param config The DHT node configuration
     */
    explicit DHTTransactionManager(const DHTNodeConfig& config);

    /**
     * @brief Destructor
     */
    ~DHTTransactionManager();

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
     * @brief Creates a new transaction
     * @param query The query message
     * @param endpoint The endpoint to send the query to
     * @param responseCallback The callback to call when a response is received
     * @param errorCallback The callback to call when an error is received
     * @param timeoutCallback The callback to call when the transaction times out
     * @return The transaction ID, or an empty string if the transaction could not be created
     */
    std::string createTransaction(std::shared_ptr<QueryMessage> query,
                                 const network::EndPoint& endpoint,
                                 ResponseCallback responseCallback = nullptr,
                                 ErrorCallback errorCallback = nullptr,
                                 TimeoutCallback timeoutCallback = nullptr);

    /**
     * @brief Finds a transaction by ID
     * @param id The transaction ID
     * @return The transaction, or nullptr if not found
     */
    std::shared_ptr<Transaction> findTransaction(const std::string& id);

    /**
     * @brief Removes a transaction
     * @param id The transaction ID
     * @return True if the transaction was removed, false otherwise
     */
    bool removeTransaction(const std::string& id);

    /**
     * @brief Handles a response message
     * @param response The response message
     * @param sender The sender's endpoint
     * @return True if the response was handled, false otherwise
     */
    bool handleResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender);

    /**
     * @brief Handles an error message
     * @param error The error message
     * @param sender The sender's endpoint
     * @return True if the error was handled, false otherwise
     */
    bool handleError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender);

    /**
     * @brief Checks for timed out transactions
     */
    void checkTimeouts();

    /**
     * @brief Saves transactions to a file
     * @param filePath The path to the file
     * @return True if the transactions were saved successfully, false otherwise
     */
    bool saveTransactions(const std::string& filePath) const;

    /**
     * @brief Loads transactions from a file
     * @param filePath The path to the file
     * @return True if the transactions were loaded successfully, false otherwise
     */
    bool loadTransactions(const std::string& filePath);

private:
    /**
     * @brief Generates a random transaction ID
     * @return The transaction ID
     */
    std::string generateTransactionID() const;

    /**
     * @brief Checks for timed out transactions periodically
     */
    void checkTimeoutsPeriodically();

    /**
     * @brief Saves transactions periodically
     */
    void saveTransactionsPeriodically();

    DHTNodeConfig m_config;
    std::string m_transactionsPath;
    std::unordered_map<std::string, std::shared_ptr<Transaction>> m_transactions;
    mutable util::CheckedMutex m_transactionsMutex;
    std::atomic<bool> m_running{false};
    std::thread m_timeoutThread;
    std::thread m_saveThread;
};

} // namespace dht_hunter::dht
