#include "dht_hunter/dht/transactions/components/transaction_manager.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"
#include "dht_hunter/utility/random/random_utils.hpp"
#include <algorithm>

namespace dht_hunter::dht::transactions {

// Initialize static members
std::shared_ptr<TransactionManager> TransactionManager::s_instance = nullptr;
std::mutex TransactionManager::s_instanceMutex;

std::shared_ptr<TransactionManager> TransactionManager::getInstance(const DHTConfig& config, const NodeID& nodeID) {
    try {
        return utility::thread::withLock(s_instanceMutex, [&config, &nodeID]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<TransactionManager>(new TransactionManager(config, nodeID));
            }
            return s_instance;
        }, "TransactionManager::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Transactions.TransactionManager", e.what());
        return nullptr;
    }
}

TransactionManager::TransactionManager(const DHTConfig& config, const NodeID& nodeID)
    : BaseTransactionComponent("TransactionManager", config, nodeID),
      m_rng(std::random_device{}()),
      m_maxTransactions(calculateMaxTransactions()) {

    // Get transaction timeout from configuration
    auto configManager = utility::config::ConfigurationManager::getInstance();
    m_transactionTimeout = configManager->getInt("network.transactionTimeout", 30);

    // Get max transactions from configuration if specified
    if (configManager->hasKey("network.maxTransactions")) {
        m_maxTransactions = static_cast<size_t>(configManager->getInt("network.maxTransactions", static_cast<int>(m_maxTransactions)));
    }

    unified_event::logInfo("DHT.Transactions." + m_name,
                          "Initialized with transaction timeout: " + std::to_string(m_transactionTimeout) +
                          " seconds, max transactions: " + std::to_string(m_maxTransactions));
}

TransactionManager::~TransactionManager() {
    // Stop the component
    stop();

    // Clear the singleton instance
    try {
        utility::thread::withLock(s_instanceMutex, [this]() {
            if (s_instance.get() == this) {
                s_instance.reset();
            }
        }, "TransactionManager::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Transactions." + m_name, e.what());
    }
}

bool TransactionManager::onInitialize() {
    return true;
}

bool TransactionManager::onStart() {
    // Start the timeout thread
    m_timeoutThread = std::thread(&TransactionManager::checkTimeoutsPeriodically, this);
    return true;
}

void TransactionManager::onStop() {
    // Stop the timeout thread
    if (m_timeoutThread.joinable()) {
        m_timeoutThread.join();
    }

    // Clear all transactions
    try {
        utility::thread::withLock(m_mutex, [this]() {
            m_transactions.clear();
        }, "TransactionManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Transactions." + m_name, e.what());
    }
}

std::string TransactionManager::createTransaction(std::shared_ptr<QueryMessage> query,
                                                const EndPoint& endpoint,
                                                TransactionResponseCallback responseCallback,
                                                TransactionErrorCallback errorCallback,
                                                TransactionTimeoutCallback timeoutCallback,
                                                std::shared_ptr<void> context) {
    try {
        return utility::thread::withLock(m_mutex, [this, &query, &endpoint, &responseCallback, &errorCallback, &timeoutCallback, &context]() {
            // Check if we have too many transactions
            if (m_transactions.size() >= m_maxTransactions) {
                unified_event::logWarning("DHT.Transactions." + m_name,
                    "Too many transactions (" + std::to_string(m_transactions.size()) + "/" +
                    std::to_string(m_maxTransactions) + "), dropping new transaction");
                return std::string("");
            }

            // Generate a transaction ID
            std::string transactionID = generateTransactionID();

            // Set the transaction ID in the query
            query->setTransactionID(transactionID);

            // Create the transaction
            Transaction transaction(transactionID, query, endpoint, responseCallback, errorCallback, timeoutCallback);

            // Set the context if provided
            if (context) {
                transaction.setContext(context);
            }

            // Add the transaction
            m_transactions.emplace(transactionID, transaction);

            unified_event::logDebug("DHT.Transactions." + m_name, "Created transaction " + transactionID + " for endpoint " + endpoint.toString());

            return transactionID;
        }, "TransactionManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Transactions." + m_name, e.what());
        return "";
    }
}

void TransactionManager::handleResponse(std::shared_ptr<ResponseMessage> response, const EndPoint& sender) {
    if (!response) {
        return;
    }

    // Get the transaction ID
    const std::string& transactionID = response->getTransactionID();

    // Store the callback to call it outside the lock
    TransactionResponseCallback callback;

    try {
        utility::thread::withLock(m_mutex, [this, &response, &callback]() {
            // Get the transaction ID
            const std::string& transactionID = response->getTransactionID();

            // Find the transaction
            auto it = m_transactions.find(transactionID);
            if (it == m_transactions.end()) {
                unified_event::logDebug("DHT.Transactions." + m_name, "Received response for unknown transaction " + transactionID);
                return;
            }

            // Get the transaction
            Transaction& transaction = it->second;

            // Store the callback to call it outside the lock
            callback = transaction.getResponseCallback();

            // Remove the transaction
            m_transactions.erase(it);

            unified_event::logDebug("DHT.Transactions." + m_name, "Handled response for transaction " + transactionID);
        }, "TransactionManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Transactions." + m_name, e.what());
        return;
    }

    // Call the callback outside the lock to avoid deadlocks
    if (callback) {
        callback(response, sender);
    }
}

void TransactionManager::handleError(std::shared_ptr<ErrorMessage> error, const EndPoint& sender) {
    if (!error) {
        return;
    }

    // Get the transaction ID
    const std::string& transactionID = error->getTransactionID();

    // Store the callback to call it outside the lock
    TransactionErrorCallback callback;

    try {
        utility::thread::withLock(m_mutex, [this, &error, &callback]() {
            // Get the transaction ID
            const std::string& transactionID = error->getTransactionID();

            // Find the transaction
            auto it = m_transactions.find(transactionID);
            if (it == m_transactions.end()) {
                unified_event::logDebug("DHT.Transactions." + m_name, "Received error for unknown transaction " + transactionID);
                return;
            }

            // Get the transaction
            Transaction& transaction = it->second;

            // Store the callback to call it outside the lock
            callback = transaction.getErrorCallback();

            // Remove the transaction
            m_transactions.erase(it);

            unified_event::logDebug("DHT.Transactions." + m_name, "Handled error for transaction " + transactionID);
        }, "TransactionManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Transactions." + m_name, e.what());
        return;
    }

    // Call the callback outside the lock to avoid deadlocks
    if (callback) {
        callback(error, sender);
    }
}

size_t TransactionManager::getTransactionCount() const {
    try {
        return utility::thread::withLock(m_mutex, [this]() {
            return m_transactions.size();
        }, "TransactionManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Transactions." + m_name, e.what());
        return 0;
    }
}

std::string TransactionManager::generateTransactionID() {
    // Use the utility function to generate a random transaction ID
    return utility::random::generateTransactionID();
}

void TransactionManager::checkTimeouts() {
    // Create a vector to store callbacks to be executed after releasing the lock
    std::vector<TransactionTimeoutCallback> timeoutCallbacks;

    try {
        utility::thread::withLock(m_mutex, [this, &timeoutCallbacks]() {
            // Check for timed out transactions
            for (auto it = m_transactions.begin(); it != m_transactions.end();) {
                const Transaction& transaction = it->second;

                // Check if the transaction has timed out
                if (transaction.hasTimedOut(m_transactionTimeout)) {
                    unified_event::logDebug("DHT.Transactions." + m_name, "Transaction " + transaction.getID() + " timed out");

                    // Store the timeout callback for later execution
                    if (transaction.getTimeoutCallback()) {
                        timeoutCallbacks.push_back(transaction.getTimeoutCallback());
                    }

                    // Remove the transaction
                    it = m_transactions.erase(it);
                } else {
                    ++it;
                }
            }
        }, "TransactionManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.Transactions." + m_name, e.what());
        return;
    }

    // Execute the timeout callbacks outside the lock
    for (const auto& callback : timeoutCallbacks) {
        callback();
    }
}

void TransactionManager::checkTimeoutsPeriodically() {
    while (m_running.load(std::memory_order_acquire)) {
        // Sleep for a while
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Check for timed out transactions
        checkTimeouts();
    }
}

} // namespace dht_hunter::dht::transactions
