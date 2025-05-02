#pragma once

#include "dht_hunter/dht/transactions/components/base_transaction_component.hpp"
#include "dht_hunter/dht/transactions/components/transaction.hpp"
#include "dht_hunter/utility/config/configuration_manager.hpp"
#include "dht_hunter/utility/system/memory_utils.hpp"
#include <unordered_map>
#include <thread>
#include <random>

namespace dht_hunter::dht::transactions {

/**
 * @brief Manager for DHT transactions
 *
 * This component manages DHT transactions, including creating, tracking, and timing out transactions.
 */
class TransactionManager : public BaseTransactionComponent {
public:
    /**
     * @brief Gets the singleton instance
     * @param config The DHT configuration
     * @param nodeID The node ID
     * @return The singleton instance
     */
    static std::shared_ptr<TransactionManager> getInstance(const DHTConfig& config, const NodeID& nodeID);

    /**
     * @brief Destructor
     */
    ~TransactionManager() override;

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
                                 const EndPoint& endpoint,
                                 TransactionResponseCallback responseCallback,
                                 TransactionErrorCallback errorCallback,
                                 TransactionTimeoutCallback timeoutCallback);

    /**
     * @brief Handles a response message
     * @param response The response message
     * @param sender The sender
     */
    void handleResponse(std::shared_ptr<ResponseMessage> response, const EndPoint& sender);

    /**
     * @brief Handles an error message
     * @param error The error message
     * @param sender The sender
     */
    void handleError(std::shared_ptr<ErrorMessage> error, const EndPoint& sender);

    /**
     * @brief Gets the number of active transactions
     * @return The number of active transactions
     */
    size_t getTransactionCount() const;

protected:
    /**
     * @brief Initializes the component
     * @return True if the component was initialized successfully, false otherwise
     */
    bool onInitialize() override;

    /**
     * @brief Starts the component
     * @return True if the component was started successfully, false otherwise
     */
    bool onStart() override;

    /**
     * @brief Stops the component
     */
    void onStop() override;

private:
    /**
     * @brief Private constructor for singleton pattern
     * @param config The DHT configuration
     * @param nodeID The node ID
     */
    TransactionManager(const DHTConfig& config, const NodeID& nodeID);

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

    // Static members
    static std::shared_ptr<TransactionManager> s_instance;
    static std::mutex s_instanceMutex;

    // Transaction storage
    std::unordered_map<std::string, Transaction> m_transactions;

    // Timeout thread
    std::thread m_timeoutThread;

    // Random number generator
    std::mt19937 m_rng;

    // Maximum number of transactions (calculated once during initialization)
    size_t m_maxTransactions;

    // Transaction timeout in seconds (configurable)
    int m_transactionTimeout;

    // Maximum number of transactions is dynamically calculated based on available memory
    // This is just a fallback value if memory detection fails
    static constexpr size_t DEFAULT_MAX_TRANSACTIONS = 100000;

    // Calculate the maximum number of transactions based on available memory
    // Uses 25% of available memory by default
    static size_t calculateMaxTransactions() {
        return utility::system::calculateMaxTransactions(
            0.25,                  // Use 25% of available memory
            350,                   // Estimated bytes per transaction
            1000,                  // Minimum transactions
            1000000                // Maximum transactions
        );
    }
};

} // namespace dht_hunter::dht::transactions
