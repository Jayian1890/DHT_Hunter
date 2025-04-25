#include "dht_hunter/dht/transactions/dht_transaction_manager.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include "dht_hunter/bencode/bencode.hpp"
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <iomanip>

DEFINE_COMPONENT_LOGGER("DHT", "TransactionManager")

namespace dht_hunter::dht {

//
// Transaction implementation
//

Transaction::Transaction(const std::string& id,
                       std::shared_ptr<QueryMessage> query,
                       const network::EndPoint& endpoint,
                       ResponseCallback responseCallback,
                       ErrorCallback errorCallback,
                       TimeoutCallback timeoutCallback)
    : m_id(id),
      m_query(query),
      m_endpoint(endpoint),
      m_creationTime(std::chrono::steady_clock::now()),
      m_responseCallback(responseCallback),
      m_errorCallback(errorCallback),
      m_timeoutCallback(timeoutCallback) {
}

const std::string& Transaction::getID() const {
    return m_id;
}

std::shared_ptr<QueryMessage> Transaction::getQuery() const {
    return m_query;
}

const network::EndPoint& Transaction::getEndpoint() const {
    return m_endpoint;
}

std::chrono::steady_clock::time_point Transaction::getCreationTime() const {
    return m_creationTime;
}

bool Transaction::hasTimedOut(int timeout) const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_creationTime).count();
    return elapsed >= timeout;
}

void Transaction::handleResponse(std::shared_ptr<ResponseMessage> response) {
    if (m_responseCallback) {
        m_responseCallback(response);
    }
}

void Transaction::handleError(std::shared_ptr<ErrorMessage> error) {
    if (m_errorCallback) {
        m_errorCallback(error);
    }
}

void Transaction::handleTimeout() {
    if (m_timeoutCallback) {
        m_timeoutCallback();
    }
}

//
// DHTTransactionManager implementation
//

DHTTransactionManager::DHTTransactionManager(const DHTNodeConfig& config)
    : m_config(config),
      m_transactionsPath(config.getTransactionsPath().empty() ?
                        config.getConfigDir() + "/transactions.dat" :
                        config.getTransactionsPath()),
      m_running(false) {
    getLogger()->info("Transaction manager created");
}

DHTTransactionManager::~DHTTransactionManager() {
    stop();
}

bool DHTTransactionManager::start() {
    if (m_running) {
        getLogger()->warning("Transaction manager already running");
        return false;
    }

    // Load transactions if configured to do so
    if (m_config.getLoadTransactionsOnStartup() && std::filesystem::exists(m_transactionsPath)) {
        getLogger()->info("Loading transactions from {}", m_transactionsPath);
        if (!loadTransactions(m_transactionsPath)) {
            getLogger()->warning("Failed to load transactions from {}", m_transactionsPath);
        }
    }

    // Start the timeout thread
    m_running = true;
    m_timeoutThread = std::thread(&DHTTransactionManager::checkTimeoutsPeriodically, this);
    m_saveThread = std::thread(&DHTTransactionManager::saveTransactionsPeriodically, this);

    getLogger()->info("Transaction manager started");
    return true;
}

void DHTTransactionManager::stop() {
    // Use atomic operation to prevent multiple stops
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false)) {
        // Already stopped or stopping
        return;
    }

    getLogger()->info("Stopping transaction manager");

    // Save transactions if configured to do so
    if (m_config.getSaveTransactionsOnShutdown()) {
        saveTransactions(m_transactionsPath);
    }

    // Join the timeout thread with timeout
    if (m_timeoutThread.joinable()) {
        getLogger()->debug("Waiting for timeout thread to join...");

        // Try to join with timeout
        auto joinStartTime = std::chrono::steady_clock::now();
        auto threadJoinTimeout = std::chrono::seconds(DEFAULT_THREAD_JOIN_TIMEOUT_SECONDS);

        std::thread tempThread([this]() {
            if (m_timeoutThread.joinable()) {
                m_timeoutThread.join();
            }
        });

        // Wait for the join thread with timeout
        if (tempThread.joinable()) {
            bool joined = false;
            while (!joined &&
                   std::chrono::steady_clock::now() - joinStartTime < threadJoinTimeout) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                joined = !m_timeoutThread.joinable();
            }

            if (joined) {
                tempThread.join();
                getLogger()->debug("Timeout thread joined successfully");
            } else {
                // If we couldn't join, detach both threads to avoid blocking
                getLogger()->warning("Timeout thread join timed out after {} seconds, detaching",
                              threadJoinTimeout.count());
                tempThread.detach();
                // We can't safely join the original thread now, so we detach it
                if (m_timeoutThread.joinable()) {
                    m_timeoutThread.detach();
                }
            }
        }
    }

    // Join the save thread with timeout
    if (m_saveThread.joinable()) {
        getLogger()->debug("Waiting for save thread to join...");

        // Try to join with timeout
        auto joinStartTime = std::chrono::steady_clock::now();
        auto threadJoinTimeout = std::chrono::seconds(DEFAULT_THREAD_JOIN_TIMEOUT_SECONDS);

        std::thread tempThread([this]() {
            if (m_saveThread.joinable()) {
                m_saveThread.join();
            }
        });

        // Wait for the join thread with timeout
        if (tempThread.joinable()) {
            bool joined = false;
            while (!joined &&
                   std::chrono::steady_clock::now() - joinStartTime < threadJoinTimeout) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                joined = !m_saveThread.joinable();
            }

            if (joined) {
                tempThread.join();
                getLogger()->debug("Save thread joined successfully");
            } else {
                // If we couldn't join, detach both threads to avoid blocking
                getLogger()->warning("Save thread join timed out after {} seconds, detaching",
                              threadJoinTimeout.count());
                tempThread.detach();
                // We can't safely join the original thread now, so we detach it
                if (m_saveThread.joinable()) {
                    m_saveThread.detach();
                }
            }
        }
    }

    getLogger()->info("Transaction manager stopped");
}

bool DHTTransactionManager::isRunning() const {
    return m_running;
}

std::string DHTTransactionManager::createTransaction(std::shared_ptr<QueryMessage> query,
                                                  const network::EndPoint& endpoint,
                                                  ResponseCallback responseCallback,
                                                  ErrorCallback errorCallback,
                                                  TimeoutCallback timeoutCallback) {
    if (!m_running) {
        getLogger()->error("Cannot create transaction: Transaction manager not running");
        return "";
    }

    if (!query) {
        getLogger()->error("Cannot create transaction: Invalid query");
        return "";
    }

    // Generate a transaction ID
    std::string transactionID = generateTransactionID();

    // Set the transaction ID in the query
    query->setTransactionID(transactionID);

    // Create the transaction
    auto transaction = std::make_shared<Transaction>(
        transactionID,
        query,
        endpoint,
        responseCallback,
        errorCallback,
        timeoutCallback
    );

    // Add the transaction to the map
    {
        std::lock_guard<util::CheckedMutex> lock(m_transactionsMutex);

        // Check if we have too many transactions
        if (m_transactions.size() >= MAX_TRANSACTIONS) {
            getLogger()->error("Cannot create transaction: Too many transactions");
            return "";
        }

        m_transactions[transactionID] = transaction;
    }

    getLogger()->debug("Created transaction {} for {} query to {}",
                 transactionID,
                 queryMethodToString(query->getMethod()),
                 endpoint.toString());

    return transactionID;
}

std::shared_ptr<Transaction> DHTTransactionManager::findTransaction(const std::string& id) {
    std::lock_guard<util::CheckedMutex> lock(m_transactionsMutex);

    auto it = m_transactions.find(id);
    if (it == m_transactions.end()) {
        return nullptr;
    }

    return it->second;
}

bool DHTTransactionManager::hasTransaction(const std::string& id) {
    std::lock_guard<util::CheckedMutex> lock(m_transactionsMutex);
    return m_transactions.find(id) != m_transactions.end();
}

bool DHTTransactionManager::removeTransaction(const std::string& id) {
    std::lock_guard<util::CheckedMutex> lock(m_transactionsMutex);

    auto it = m_transactions.find(id);
    if (it == m_transactions.end()) {
        return false;
    }

    m_transactions.erase(it);
    return true;
}

bool DHTTransactionManager::handleResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender) {
    if (!response) {
        getLogger()->error("Cannot handle response: Invalid response");
        return false;
    }

    // Find the transaction
    auto transaction = findTransaction(response->getTransactionID());
    if (!transaction) {
        getLogger()->warning("Cannot handle response: Transaction {} not found", response->getTransactionID());
        return false;
    }

    // Check if the response is from the expected endpoint
    if (transaction->getEndpoint() != sender) {
        getLogger()->warning("Response for transaction {} received from unexpected endpoint: {} (expected {})",
                      response->getTransactionID(),
                      sender.toString(),
                      transaction->getEndpoint().toString());
        // We still handle the response, as it might be a valid response from a different endpoint
    }

    // Handle the response
    transaction->handleResponse(response);

    // Remove the transaction
    removeTransaction(response->getTransactionID());

    return true;
}

bool DHTTransactionManager::handleError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender) {
    if (!error) {
        getLogger()->error("Cannot handle error: Invalid error");
        return false;
    }

    // Find the transaction
    auto transaction = findTransaction(error->getTransactionID());
    if (!transaction) {
        getLogger()->warning("Cannot handle error: Transaction {} not found", error->getTransactionID());
        return false;
    }

    // Check if the error is from the expected endpoint
    if (transaction->getEndpoint() != sender) {
        getLogger()->warning("Error for transaction {} received from unexpected endpoint: {} (expected {})",
                      error->getTransactionID(),
                      sender.toString(),
                      transaction->getEndpoint().toString());
        // We still handle the error, as it might be a valid error from a different endpoint
    }

    // Handle the error
    transaction->handleError(error);

    // Remove the transaction
    removeTransaction(error->getTransactionID());

    return true;
}

void DHTTransactionManager::checkTimeouts() {
    std::vector<std::shared_ptr<Transaction>> timedOutTransactions;

    // Find timed out transactions
    {
        std::lock_guard<util::CheckedMutex> lock(m_transactionsMutex);

        for (auto it = m_transactions.begin(); it != m_transactions.end();) {
            if (it->second->hasTimedOut(TRANSACTION_TIMEOUT)) {
                timedOutTransactions.push_back(it->second);
                it = m_transactions.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Handle timed out transactions
    for (const auto& transaction : timedOutTransactions) {
        getLogger()->debug("Transaction {} timed out", transaction->getID());
        transaction->handleTimeout();
    }
}

// The saveTransactions method is now implemented in dht_transaction_persistence.cpp

// The loadTransactions method is now implemented in dht_transaction_persistence.cpp

std::string DHTTransactionManager::generateTransactionID() const {
    // Generate a random transaction ID
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<uint32_t> dist;

    // Generate a random 32-bit integer
    uint32_t random = dist(rng);

    // Convert to a hex string
    std::stringstream ss;
    ss << std::hex << std::setw(8) << std::setfill('0') << random;

    return ss.str();
}

void DHTTransactionManager::checkTimeoutsPeriodically() {
    getLogger()->debug("Starting periodic transaction timeout check thread");

    while (m_running) {
        // Sleep for a short time
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Check if we should still be running
        if (!m_running) {
            break;
        }

        // Check for timed out transactions
        checkTimeouts();
    }

    getLogger()->debug("Periodic transaction timeout check thread exiting");
}

void DHTTransactionManager::saveTransactionsPeriodically() {
    getLogger()->debug("Starting periodic transaction save thread");

    while (m_running) {
        // Sleep for the save interval
        std::this_thread::sleep_for(std::chrono::seconds(TRANSACTION_SAVE_INTERVAL));

        // Check if we should still be running
        if (!m_running) {
            break;
        }

        // Save the transactions
        saveTransactions(m_transactionsPath);
    }

    getLogger()->debug("Periodic transaction save thread exiting");
}

} // namespace dht_hunter::dht
