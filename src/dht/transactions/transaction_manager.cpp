#include "dht_hunter/dht/transactions/transaction_manager.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/utils/lock_utils.hpp"
#include <sstream>
#include <iomanip>

namespace dht_hunter::dht {

// Initialize static members
std::shared_ptr<TransactionManager> TransactionManager::s_instance = nullptr;
std::mutex TransactionManager::s_instanceMutex;

std::shared_ptr<TransactionManager> TransactionManager::getInstance(const DHTConfig& config) {
    try {
        return utils::withLock(s_instanceMutex, [&config]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<TransactionManager>(new TransactionManager(config));
            }
            return s_instance;
        }, "TransactionManager::s_instanceMutex");
    } catch (const utils::LockTimeoutException& e) {
        unified_event::logError("DHT.TransactionManager", e.what());
        return nullptr;
    }
}

TransactionManager::TransactionManager(const DHTConfig& config)
    : m_config(config), m_running(false), m_rng(std::random_device{}()) {
}

TransactionManager::~TransactionManager() {
    stop();

    // Clear the singleton instance
    try {
        utils::withLock(s_instanceMutex, [this]() {
            if (s_instance.get() == this) {
                s_instance.reset();
            }
        }, "TransactionManager::s_instanceMutex");
    } catch (const utils::LockTimeoutException& e) {
        unified_event::logError("DHT.TransactionManager", e.what());
    }
}

bool TransactionManager::start() {
    try {
        bool shouldStartThread = utils::withLock(m_mutex, [this]() {
            if (m_running) {
                return false; // Thread already running
            }

            m_running = true;
            return true; // Need to start thread
        }, "TransactionManager::m_mutex");

        // Start the timeout thread if needed
        if (shouldStartThread) {
            m_timeoutThread = std::thread(&TransactionManager::checkTimeoutsPeriodically, this);
        }

        return true;
    } catch (const utils::LockTimeoutException& e) {
        unified_event::logError("DHT.TransactionManager", e.what());
        return false;
    }
}

void TransactionManager::stop() {
    bool shouldJoinThread = false;

    try {
        shouldJoinThread = utils::withLock(m_mutex, [this]() {
            if (!m_running) {
                return false; // No thread to join
            }

            m_running = false;
            return true; // Need to join thread
        }, "TransactionManager::m_mutex");
    } catch (const utils::LockTimeoutException& e) {
        unified_event::logError("DHT.TransactionManager", e.what());
        return;
    }

    // Only join the thread if we actually stopped it
    if (shouldJoinThread && m_timeoutThread.joinable()) {
        m_timeoutThread.join();
    }
}

bool TransactionManager::isRunning() const {
    return m_running.load(std::memory_order_acquire);
}

std::string TransactionManager::createTransaction(std::shared_ptr<QueryMessage> query,
                                                const network::EndPoint& endpoint,
                                                TransactionResponseCallback responseCallback,
                                                TransactionErrorCallback errorCallback,
                                                TransactionTimeoutCallback timeoutCallback) {
    try {
        return utils::withLock(m_mutex, [this, &query, &endpoint, &responseCallback, &errorCallback, &timeoutCallback]() {
            // Check if we have too many transactions
            if (m_transactions.size() >= MAX_TRANSACTIONS) {
                return std::string("");
            }

            // Generate a transaction ID
            std::string transactionID = generateTransactionID();

            // Set the transaction ID in the query
            query->setTransactionID(transactionID);

            // Create the transaction
            Transaction transaction(transactionID, query, endpoint, responseCallback, errorCallback, timeoutCallback);

            // Add the transaction
            m_transactions.emplace(transactionID, transaction);

            return transactionID;
        }, "TransactionManager::m_mutex");
    } catch (const utils::LockTimeoutException& e) {
        unified_event::logError("DHT.TransactionManager", e.what());
        return "";
    }
}

void TransactionManager::handleResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender) {
    // Store the callback outside the lock to avoid calling it while holding the lock
    TransactionResponseCallback callback = nullptr;

    try {
        utils::withLock(m_mutex, [this, &response, &callback]() {
            // Get the transaction ID
            const std::string& transactionID = response->getTransactionID();

            // Find the transaction
            auto it = m_transactions.find(transactionID);
            if (it == m_transactions.end()) {
                return;
            }

            // Get the transaction
            Transaction& transaction = it->second;

            // Store the callback to call it outside the lock
            callback = transaction.responseCallback;

            // Remove the transaction
            m_transactions.erase(it);
        }, "TransactionManager::m_mutex");
    } catch (const utils::LockTimeoutException& e) {
        unified_event::logError("DHT.TransactionManager", e.what());
        return;
    }

    // Call the callback outside the lock to avoid deadlocks
    if (callback) {
        callback(response, sender);
    }
}

void TransactionManager::handleError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender) {
    // Store the callback outside the lock to avoid calling it while holding the lock
    TransactionErrorCallback callback = nullptr;

    try {
        utils::withLock(m_mutex, [this, &error, &callback]() {
            // Get the transaction ID
            const std::string& transactionID = error->getTransactionID();

            // Find the transaction
            auto it = m_transactions.find(transactionID);
            if (it == m_transactions.end()) {
                return;
            }

            // Get the transaction
            Transaction& transaction = it->second;

            // Store the callback to call it outside the lock
            callback = transaction.errorCallback;

            // Remove the transaction
            m_transactions.erase(it);
        }, "TransactionManager::m_mutex");
    } catch (const utils::LockTimeoutException& e) {
        unified_event::logError("DHT.TransactionManager", e.what());
        return;
    }

    // Call the callback outside the lock to avoid deadlocks
    if (callback) {
        callback(error, sender);
    }
}

size_t TransactionManager::getTransactionCount() const {
    try {
        return utils::withLock(m_mutex, [this]() {
            return m_transactions.size();
        }, "TransactionManager::m_mutex");
    } catch (const utils::LockTimeoutException& e) {
        unified_event::logError("DHT.TransactionManager", e.what());
        return 0;
    }
}

std::string TransactionManager::generateTransactionID() {
    // Generate a random transaction ID
    std::uniform_int_distribution<> dis(0, 255);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (size_t i = 0; i < 4; ++i) {
        ss << std::setw(2) << dis(m_rng);
    }

    return ss.str();
}

void TransactionManager::checkTimeouts() {
    // Create a vector to store callbacks to be executed after releasing the lock
    std::vector<std::function<void()>> timeoutCallbacks;

    try {
        utils::withLock(m_mutex, [this, &timeoutCallbacks]() {
            auto now = std::chrono::steady_clock::now();

            // Check for timed out transactions
            for (auto it = m_transactions.begin(); it != m_transactions.end();) {
                const Transaction& transaction = it->second;

                // Calculate the elapsed time
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - transaction.timestamp).count();

                // Check if the transaction has timed out
                if (elapsed >= TRANSACTION_TIMEOUT) {

                    // Store the timeout callback for later execution
                    if (transaction.timeoutCallback) {
                        timeoutCallbacks.push_back(transaction.timeoutCallback);
                    }

                    // Remove the transaction
                    it = m_transactions.erase(it);
                } else {
                    ++it;
                }
            }
        }, "TransactionManager::m_mutex");
    } catch (const utils::LockTimeoutException& e) {
        unified_event::logError("DHT.TransactionManager", e.what());
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

} // namespace dht_hunter::dht
