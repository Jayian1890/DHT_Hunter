#pragma once

#include "dht_hunter/dht/transactions/components/transaction_manager.hpp"
#include "dht_hunter/dht/transactions/components/transaction.hpp"

namespace dht_hunter::dht {

// Type aliases for backward compatibility
using TransactionResponseCallback = transactions::TransactionResponseCallback;
using TransactionErrorCallback = transactions::TransactionErrorCallback;
using TransactionTimeoutCallback = transactions::TransactionTimeoutCallback;
using Transaction = transactions::Transaction;

/**
 * @brief Manages DHT transactions (Singleton)
 */
class TransactionManager : public transactions::TransactionManager {
public:
    /**
     * @brief Gets the singleton instance of the transaction manager
     * @param config The DHT configuration (only used if instance doesn't exist yet)
     * @return The singleton instance
     */
    static std::shared_ptr<TransactionManager> getInstance(const DHTConfig& config) {
        auto instance = transactions::TransactionManager::getInstance(config, NodeID());
        return std::static_pointer_cast<TransactionManager>(instance);
    }
};

} // namespace dht_hunter::dht
