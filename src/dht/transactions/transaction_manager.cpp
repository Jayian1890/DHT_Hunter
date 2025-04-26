#include "dht_hunter/dht/transactions/transaction_manager.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include <sstream>
#include <iomanip>

namespace dht_hunter::dht {

// Initialize static members
std::shared_ptr<TransactionManager> TransactionManager::s_instance = nullptr;
std::mutex TransactionManager::s_instanceMutex;

std::shared_ptr<TransactionManager> TransactionManager::getInstance(const DHTConfig& config) {
    std::lock_guard<std::mutex> lock(s_instanceMutex);

    if (!s_instance) {
        s_instance = std::shared_ptr<TransactionManager>(new TransactionManager(config));
    }

    return s_instance;
}

TransactionManager::TransactionManager(const DHTConfig& config)
    : m_config(config), m_running(false), m_rng(std::random_device{}()),
      m_logger(event::Logger::forComponent("DHT.TransactionManager")) {
}

TransactionManager::~TransactionManager() {
    stop();

    // Clear the singleton instance
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (s_instance.get() == this) {
        s_instance.reset();
    }
}

bool TransactionManager::start() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running) {
        m_logger.warning("Transaction manager already running");
        return true;
    }

    m_running = true;

    // Start the timeout thread
    m_timeoutThread = std::thread(&TransactionManager::checkTimeoutsPeriodically, this);

    return true;
}

void TransactionManager::stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_running) {
            return;
        }

        m_running = false;
    } // Release the lock before joining the thread

    // Wait for the timeout thread to finish
    if (m_timeoutThread.joinable()) {
        m_timeoutThread.join();
    }

    m_logger.info("Transaction manager stopped");
}

bool TransactionManager::isRunning() const {
    return m_running;
}

std::string TransactionManager::createTransaction(std::shared_ptr<QueryMessage> query,
                                                const network::EndPoint& endpoint,
                                                TransactionResponseCallback responseCallback,
                                                TransactionErrorCallback errorCallback,
                                                TransactionTimeoutCallback timeoutCallback) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if we have too many transactions
    if (m_transactions.size() >= MAX_TRANSACTIONS) {
        m_logger.error("Too many transactions");
        return "";
    }

    // Generate a transaction ID
    std::string transactionID = generateTransactionID();

    // Set the transaction ID in the query
    query->setTransactionID(transactionID);

    // Create the transaction
    Transaction transaction(transactionID, query, endpoint, responseCallback, errorCallback, timeoutCallback);

    // Add the transaction
    m_transactions.emplace(transactionID, transaction);

    m_logger.debug("Created transaction {} for {}", transactionID, endpoint.toString());

    return transactionID;
}

void TransactionManager::handleResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Get the transaction ID
    const std::string& transactionID = response->getTransactionID();

    // Find the transaction
    auto it = m_transactions.find(transactionID);
    if (it == m_transactions.end()) {
        m_logger.debug("Received response for unknown transaction {}", transactionID);
        return;
    }

    // Get the transaction
    Transaction& transaction = it->second;

    // Call the response callback
    if (transaction.responseCallback) {
        transaction.responseCallback(response, sender);
    }

    // Remove the transaction
    m_transactions.erase(it);

    m_logger.debug("Handled response for transaction {}", transactionID);
}

void TransactionManager::handleError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Get the transaction ID
    const std::string& transactionID = error->getTransactionID();

    // Find the transaction
    auto it = m_transactions.find(transactionID);
    if (it == m_transactions.end()) {
        m_logger.debug("Received error for unknown transaction {}", transactionID);
        return;
    }

    // Get the transaction
    Transaction& transaction = it->second;

    // Call the error callback
    if (transaction.errorCallback) {
        transaction.errorCallback(error, sender);
    }

    // Remove the transaction
    m_transactions.erase(it);

    m_logger.debug("Handled error for transaction {}", transactionID);
}

size_t TransactionManager::getTransactionCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_transactions.size();
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

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto now = std::chrono::steady_clock::now();

        // Check for timed out transactions
        for (auto it = m_transactions.begin(); it != m_transactions.end();) {
            const Transaction& transaction = it->second;

            // Calculate the elapsed time
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - transaction.timestamp).count();

            // Check if the transaction has timed out
            if (elapsed >= TRANSACTION_TIMEOUT) {
                m_logger.debug("Transaction {} timed out", transaction.id);

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
    } // Release the lock before calling callbacks

    // Execute the timeout callbacks outside the lock
    for (const auto& callback : timeoutCallbacks) {
        callback();
    }
}

void TransactionManager::checkTimeoutsPeriodically() {
    while (m_running) {
        // Sleep for a while
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Check for timed out transactions
        checkTimeouts();
    }
}

} // namespace dht_hunter::dht
