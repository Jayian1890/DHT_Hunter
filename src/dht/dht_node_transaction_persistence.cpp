#include "dht_hunter/dht/dht_node.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include "dht_hunter/util/mutex_utils.hpp"
#include <filesystem>
#include <thread>
#include <chrono>

DEFINE_COMPONENT_LOGGER("DHT", "Node")

namespace dht_hunter::dht {

bool DHTNode::saveTransactions(const std::string& filePath) const {
    getLogger()->debug("Saving transactions to file: {}", filePath);

    try {
        // We need to make a copy of the transactions to avoid holding the lock for too long
        std::vector<SerializableTransaction> serializableTransactions;

        {
            // Lock the transactions mutex
            std::lock_guard<util::CheckedMutex> lock(const_cast<util::CheckedMutex&>(m_transactionsMutex));

            // Reserve space for the transactions
            serializableTransactions.reserve(m_transactions.size() + m_transactionQueue.size());

            // Add active transactions
            for (const auto& [transactionIDStr, transaction] : m_transactions) {
                SerializableTransaction serializableTransaction;
                serializableTransaction.id = transaction->id;

                // Convert steady_clock timestamp to system_clock timestamp for persistence
                auto now = std::chrono::steady_clock::now();
                auto systemNow = std::chrono::system_clock::now();
                auto diff = now - transaction->timestamp;
                auto systemTimestamp = systemNow - diff;
                serializableTransaction.timestampMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    systemTimestamp.time_since_epoch()).count());

                // Determine transaction type and associated data
                // For now, we'll just save the transaction ID and timestamp
                // In a real implementation, you'd want to save more context
                serializableTransaction.type = TransactionType::Unknown;

                serializableTransactions.push_back(serializableTransaction);
            }

            // Add queued transactions (make a copy to avoid modifying the queue)
            std::queue<std::shared_ptr<Transaction>> queueCopy = m_transactionQueue;
            while (!queueCopy.empty()) {
                auto transaction = queueCopy.front();
                queueCopy.pop();

                SerializableTransaction serializableTransaction;
                serializableTransaction.id = transaction->id;

                // Convert steady_clock timestamp to system_clock timestamp for persistence
                auto now = std::chrono::steady_clock::now();
                auto systemNow = std::chrono::system_clock::now();
                auto diff = now - transaction->timestamp;
                auto systemTimestamp = systemNow - diff;
                serializableTransaction.timestampMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    systemTimestamp.time_since_epoch()).count());

                // Determine transaction type and associated data
                serializableTransaction.type = TransactionType::Unknown;

                serializableTransactions.push_back(serializableTransaction);
            }
        }

        // Save transactions
        return dht_hunter::dht::saveTransactions(filePath, serializableTransactions);
    } catch (const std::exception& e) {
        getLogger()->error("Exception while saving transactions to file: {}: {}", filePath, e.what());
        return false;
    }
}

bool DHTNode::loadTransactions(const std::string& filePath) {
    getLogger()->debug("Loading transactions from file: {}", filePath);

    try {
        // Load serializable transactions
        std::vector<SerializableTransaction> serializableTransactions;
        if (!dht_hunter::dht::loadTransactions(filePath, serializableTransactions)) {
            return false;
        }

        // Lock the transactions mutex
        std::lock_guard<util::CheckedMutex> lock(m_transactionsMutex);

        // Clear existing transactions
        m_transactions.clear();
        while (!m_transactionQueue.empty()) {
            m_transactionQueue.pop();
        }

        // Convert serializable transactions to transactions
        for (const auto& serializableTransaction : serializableTransactions) {
            // Create a new transaction
            auto transaction = std::make_shared<Transaction>();
            transaction->id = serializableTransaction.id;

            // Convert system_clock timestamp to steady_clock timestamp
            auto systemTimestamp = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(static_cast<int64_t>(serializableTransaction.timestampMs)));
            auto now = std::chrono::steady_clock::now();
            auto systemNow = std::chrono::system_clock::now();
            auto diff = systemNow - systemTimestamp;
            transaction->timestamp = now - diff;

            // Check if the transaction has expired
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - transaction->timestamp).count();
            if (elapsed > TRANSACTION_TIMEOUT) {
                getLogger()->debug("Skipping expired transaction");
                continue;
            }

            // Reconstruct callbacks based on transaction type
            // For now, we'll just create empty callbacks
            // In a real implementation, you'd want to reconstruct the callbacks based on the transaction type

            // Add the transaction
            std::string transactionIDStr(transaction->id.begin(), transaction->id.end());
            m_transactions[transactionIDStr] = transaction;
        }

        getLogger()->info("Loaded {} transactions from file: {}", m_transactions.size(), filePath);
        return true;
    } catch (const std::exception& e) {
        getLogger()->error("Exception while loading transactions from file: {}: {}", filePath, e.what());
        return false;
    }
}

void DHTNode::transactionSaveThread() {
    getLogger()->debug("Transaction save thread started");
    getLogger()->debug("Transactions path: {}", m_transactionsPath);
    getLogger()->info("Transaction save thread is disabled because transactions are saved on every change");

    // Just wait until the node is stopped
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }

    getLogger()->debug("Transaction save thread stopped");
}

} // namespace dht_hunter::dht
